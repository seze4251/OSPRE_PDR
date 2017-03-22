//
//  GNC.cpp
//  GNC
//
//  Created by Seth on 11/10/2016.
//  Copyright © 2016 Seth. All rights reserved.
//
#include <iostream>
#include <fstream>
#include <exception>
#include <stdio.h>

#include "main.h"
#include "Kalman_Filter_Iteration.h"
#include "Position_From_Angles_Slew.h"
#include "Position_From_Earth_Range.h"
#include "Position_From_Moon_Range.h"
#include "Quaternion_To_Attitude.h"
#include "Kalman_Filter_Iteration_terminate.h"
#include "Kalman_Filter_Iteration_initialize.h"
#include "GNC.h"
#include "Service.h"

GNC::GNC(std::string hostName, int localPort) : ServerInternal(hostName, localPort, P_GNC), pollTime(0) {
    setAppl(this);
    
    // Set pointers to services to NULL
    scComms = nullptr;
    cameraController = nullptr;
    
    // Initialize Pointing Destination
    point = PEM_Earth;
    
    // Allocate Memory for Messages to Send
    processHealthMessage = new ProcessHealthAndStatusResponse();
    captureImageMessage = new CaptureImageRequest();
    solutionMessage = new SolutionMessage();
    pointRequest = new PointingRequest();
    
    // Initialize localError to healthy
    localError = PE_AllHealthy;
    
    logFile = nullptr;
    
    //**************************************
    // Initialize All GNC Specific App Members
    //**************************************
    
    // GNC Specific Members
    //double x_hat[6];
    //double phi[36];
    //double P[36];
    //double dv0[3];
    //double dv1[6];
    //double dv2[9];
    //double X_est[6];
    //double covariance[36];
    //double trajectoryDev[6];
    
    //
    // From Config File
    range_EarthRangeCutoff = -1;
    range_AnglesCutoff = -1;
    r_E_SC[0] = -1;
    r_E_SC[1] = -1;
    r_E_SC[2] = -1;
    velSC[0] = -1;
    velSC[1] = -1;
    velSC[2] = -1;
    
    // Angles Method
    firstImage = true;
}

GNC::~GNC() {
    //Free Messages from Memory
    delete processHealthMessage;
    delete captureImageMessage;
    delete solutionMessage;
    delete pointRequest;
    
    // Close Log File
    if (logFile)
        fclose(logFile);
}

void GNC::open() {
    // Create File Name
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    
    rawtime = time(0);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, "./log/GNCLog_%d-%m-%Y.log",timeinfo);
    
    // Open Log File
    logFile = fopen(buffer, "a+");
    
    // Log Application Starting
    fprintf(logFile, "GNC Application Started, Time = %ld\n", time(0));
    
    // Set Timeout to 1 minute
    setTimeoutTime(10, 0);
    
    //Acceptor
    if (accept.isConnected() == false) {
        if(accept.open(hostName, localPort) == false) {
            fprintf(logFile, "Error: Unable to Open Acceptor, Exiting...\n");
            exit(-1);
        }
        fprintf(logFile, "Connection: Server Socket Opened\n");
    }
    
    //Connect to ScComms
    if(connectToAppl(hostName, 7000, &scComms) == true) {
        fprintf(logFile, "Connection: Connected to ScComms\n");
    } else {
        fprintf(logFile, "Error: Unable to Connect to ScComms\n");
        localError = PE_notConnected;
    }
    
    // Connect to Camera Controller
    if(connectToAppl(hostName, 10000, &cameraController) == true) {
        fprintf(logFile, "Connection: Connected to Camera Controller\n");
    } else {
        fprintf(logFile, "Error: Unable to Connect to CameraController\n");
        localError = PE_notConnected;
    }
    
    // Read Reference Trajectory File
    fprintf(logFile, "Reading Reference Trajectory: Attempting to Read Reference Trajectory\n");
    try {
        read_referencTraj("Text_Data/Skyfire_J2000_7_ECI_Epsecs.txt");
        fprintf(logFile, "Reference Trajectory First Row: time = %f X=%f Y=%f Z=%f V_X=%f V_Y=%f V_Z=%f \n", ref_traj.time.front(), ref_traj.X.front(), ref_traj.Y.front(), ref_traj.Z.front(), ref_traj.VX.front(), ref_traj.VY.front(), ref_traj.VZ.front());
        fprintf(logFile, "Reference Trajectory Last Row: time = %f X=%f Y=%f Z=%f V_X=%f V_Y=%f V_Z=%f \n", ref_traj.time.back(), ref_traj.X.back(), ref_traj.Y.back(), ref_traj.Z.back(), ref_traj.VX.back(), ref_traj.VY.back(), ref_traj.VZ.back());
    } catch (const char* e) {
        fprintf(logFile, "Error: ReadReferenceTrajectory Exception Caught: %s\n",e);
        throw;
        
    } catch(std::exception &exception) {
        fprintf(logFile, "Error: ReadReferenceTrajectory Exception Caught: %s\n", exception.what());
        throw;
        
    } catch (...) {
        fprintf(logFile, "Error: ReadReferenceTrajectory Unknown Type of Exception Caught\n");
        throw;
    }
    
    // Read Config File
    fprintf(logFile, "Reading Config File: Attempting to Configuration File\n");
    try {
        read_ConfigFile("Text_Data/GNC_Config.txt");
         fprintf(logFile, "Config File: Initial Pos: (%f,%f,%f), Earth Range Cutoff: %f, Angles Cutoff: %f\n", r_E_SC[0], r_E_SC[1], r_E_SC[2], range_EarthRangeCutoff, range_AnglesCutoff);
    } catch (const char* e) {
        fprintf(logFile, "Error: read_ConfigFile Exception Caught: %s\n",e);
        throw;
        
    } catch(std::exception &exception) {
        fprintf(logFile, "Error: read_ConfigFile Exception Caught: %s\n", exception.what());
        throw;
        
    } catch (...) {
        fprintf(logFile, "Error: read_ConfigFile Unknown Type of Exception Caught\n");
        throw;
    }
    flushLog();
}

/*
 1. Need to check that all connections are still open
 2. Need to Send Timed Pointing Requests to the Spacecraft
 3. Need to Send Timed Capture Image Requests to the Camera
 */
void GNC::handleTimeout() {
    // Check all connections are still open
    //Acceptor
    if (accept.isConnected() == false) {
        if(accept.open(hostName, localPort) == false) {
            fprintf(logFile, "Error: Unable to Open Acceptor, Exiting...\n");
            exit(-1);
        }
        fprintf(logFile, "Connection: Server Socket Opened\n");
    }
    
    //Connect to ScComms
    if (scComms == nullptr || scComms->isConnected() == false) {
        if(connectToAppl(hostName, 7000, &scComms) == true) {
            fprintf(logFile, "Connection: Connected to ScComms\n");
        } else {
            fprintf(logFile, "Error: Unable to Connect to ScComms\n");
            localError = PE_notConnected;
        }
    }
    
    // Connect to Camera Controller
    
    if (cameraController == nullptr || cameraController->isConnected() == false) {
        if(connectToAppl(hostName, 10000, &cameraController) == true) {
            fprintf(logFile, "Connection: Connected to CameraController\n");
        } else {
            fprintf(logFile, "Error: Unable to Connect to CameraController\n");
            localError = PE_notConnected;
        }
    }
    
    
    // Send Timed Capture Image Requests to Camera
    time_t currentTime = time(NULL);
    
    // Send Poll
    if (currentTime > pollTime) {
        if ((scComms != nullptr) && (scComms -> isConnected())) {
            pointRequest->update(point);
            scComms -> sendMessage(pointRequest);
            fprintf(logFile, "Sent Message: Pointing Request to ScComms\n");
        }
        
        if ((cameraController != nullptr) && (cameraController->isConnected())) {
            captureImageMessage->update(point, r_E_SC);
            cameraController -> sendMessage(captureImageMessage);
            fprintf(logFile, "Sent Message: CaptureImageMessage to CameraController\n");
        }
        
        pollTime = currentTime + 20;
    }
    flushLog();
}

// *******************************
//
// Application Functionality:
//
// ********************************

// TODO: Waiting On Cameron to Complete
void GNC::computeSolution(DataMessage* dataMessage, ProcessedImageMessage* procMessage) {
    std::cout << "Starting Compute Solution" << std::endl;
    if (norm(r_E_SC) < range_EarthRangeCutoff) {
        std::cout << "Start: Earth Ranging" << std::endl;
        fprintf(logFile, "ComputeSolution: Earth Ranging\n");
        
        // Set Pointing
        point = PEM_Earth;
        
        get_Reference_Trajectory(X_ref, ref_traj, procMessage->timeStamp);
        // Log Reference Trajectory
        fprintf(logFile, "ComputeSolution: Earth Ranging: Reference Trajectory: [0] %f, [1] %f, [2] %f, [3] %f, [4] %f, [5] %f \n", X_ref[0], X_ref[1], X_ref[2], X_ref[3], X_ref[4], X_ref[5]);
        
        // Log Inputs to Position_From_Earth_Range
        fprintf(logFile, "ComputeSolution: Earth Ranging: INPUTS: quat: [%f, %f, %f, %f],\n alpha = %f, beta = %f, theta = %f \n", dataMessage->quat[0], dataMessage->quat[1], dataMessage->quat[2], dataMessage->quat[3], procMessage->alpha, procMessage->beta, procMessage->theta);
        
        Position_From_Earth_Range(dataMessage->quat, procMessage->alpha, procMessage->beta, procMessage->theta, r_E_SC);
        
        std::cout << "End: Earth Ranging" << std::endl;
        
        // Log Output From Position Earth Range
        fprintf(logFile, "ComputeSolution: Earth Range: Kalman Filter Call Inputs: Time: %ld R[0] = %f, R[1] = %f, R[2] = %f\n", dataMessage->satTime, r_E_SC[0], r_E_SC[1], r_E_SC[2]);
        
        // Perform Kalman Filter Call
        std::cout << "Start: Kalman Filter Call : Earth Range" << std::endl;
        Kalman_Filter_Iteration(x_hat, phi, P, r_E_SC, R, X_ref, dataMessage->satTime, X_est);
        std::cout << "End: Kalman Filter Call : Earth Range" << std::endl;
        
        // Log Output from Kalman Filter
        fprintf(logFile, "ComputeSolution: Earth Range: Kalman Filter Results: X =  %f Y = %f, Z = %f, V_X =  %f V_Y = %f, V_Z = %f,\n", X_est[0], X_est[1], X_est[2], X_est[3], X_est[4], X_est[5]);
        
        // Find E/Sc/M Angle and Log it
        earthScMoonAngle = Earth_SC_Moon_Angle(r_E_SC, dataMessage->ephem);
        fprintf(logFile, "Compute Solution: Earth Spacecraft Moon Angle = %f\n", earthScMoonAngle);
        
        // Find Error in State and Log Outputs
        State_Error(X_ref, X_est, positionError, velocityError);
        fprintf(logFile, "Compute Solution: Position Error: X_e = %f, Y_e = %f, Z_e = %f\n Velocity Error: VX_e = %f, VY_e = %f, VZ_e = %f\n", positionError[0], positionError[1], positionError[2], velocityError[0], velocityError[1], velocityError[2]);
        
        // Update Solution Message
        std::cout << "Updateing SOlution Message" << std::endl;
        solutionMessage->update(X_est, positionError, X_est+3, velocityError, earthScMoonAngle);
        
        // Update Stored Velocity and Stored Position
        memcpy(velSC, X_est+3, 3* sizeof(double));
        memcpy(r_E_SC, X_est, 3*sizeof(double));
        
        if (scComms != nullptr) {
            std::cout << "Sending Solution Message" << std::endl;
            scComms -> sendMessage(solutionMessage);
            fprintf(logFile, "Sent Message: SolutionMessage to ScComms\n");
        }
        
    } else if ( norm(r_E_SC) < range_AnglesCutoff) {
        // Angles Method to find Position
        // Change Pointing
        if (procMessage->point == PEM_Earth) {
            point = PEM_Moon;
        } else {
            point = PEM_Earth;
        }
        
        if (firstImage == true) {
            fprintf(logFile, "ComputeSolution: Angles Method, First Image == TRUE, Saving Data and Leaving Angles Method\n");
            memcpy((void*) &dataMessage_FirstImage, (void*) dataMessage, sizeof(DataMessage));
            memcpy((void*) &procMessage_FirstImage, (void*) procMessage, sizeof(ProcessedImageMessage));
            firstImage = false;
            return;
        }
        
        fprintf(logFile, "ComputeSolution: Angles Method, First Image == FALSE, Computing Solution\n");
        
        // Need to Determine Which Messages Point At Moon / Earth
        DataMessage* dataEarth;
        DataMessage* dataMoon;
        ProcessedImageMessage* procEarth;
        ProcessedImageMessage* procMoon;
        
        if (procMessage->point == PEM_Earth) {
            dataEarth = dataMessage;
            procEarth = procMessage;
            dataMoon = &dataMessage_FirstImage;
            procMoon = &procMessage_FirstImage;
            
        } else {
            dataEarth = &dataMessage_FirstImage;
            procEarth = &procMessage_FirstImage ;
            dataMoon = dataMessage;
            procMoon = procMessage;
        }
        
        double r_E_SC1[3]; // OUTPUT(ECI Position of First Picture)
        double r_E_SC2[3]; // OUTPUT(ECI Position of Second Picture)
        
        std::cout << "Start: Position From ANGLES" << std::endl;
        Position_From_Angles_Slew(dataMoon->ephem, dataEarth->quat, dataMoon->quat, procMoon->alpha, procMoon->beta, procEarth->alpha, procEarth->beta, velSC, (double) (procMoon->timeStamp - procEarth->timeStamp), r_E_SC1, r_E_SC2);
        std::cout << "End: Position From ANGLES" << std::endl;
        
        // Kalman Filter Pic 1
        std::cout << "Start: Kalman Filter Iteration 1" << std::endl;
        
        // get and log reference trajectory
        get_Reference_Trajectory(X_ref, ref_traj, procMessage_FirstImage.timeStamp);
        fprintf(logFile, "ComputeSolution: Earth Ranging: Reference Trajectory: [0] %f, [1] %f, [2] %f, [3] %f, [4] %f, [5] %f \n", X_ref[0], X_ref[1], X_ref[2], X_ref[3], X_ref[4], X_ref[5]);
        
        Kalman_Filter_Iteration(x_hat, phi, P, r_E_SC1, R, X_ref, dataMessage_FirstImage.satTime, X_est);
        std::cout << "End: Kalman Filter Iteration 1" << std::endl;
        
        // Find E/Sc/M angle for Pic 1
        earthScMoonAngle = Earth_SC_Moon_Angle(X_est, dataMessage->ephem);
        fprintf(logFile, "Compute Solution: Earth Spacecraft Moon Angle = %f\n", earthScMoonAngle);
        
        // Find State Error for Pic 1
        State_Error(X_ref, X_est, positionError, velocityError);
        fprintf(logFile, "Compute Solution: Position Error: X_e = %f, Y_e = %f, Z_e = %f\n Velocity Error: VX_e = %f, VY_e = %f, VZ_e = %f\n", positionError[0], positionError[1], positionError[2], velocityError[0], velocityError[1], velocityError[2]);
        
        // Update Solution Message for Pic 1
        std::cout << "Updateing Solution Message" << std::endl;
        solutionMessage->update(X_est, positionError, X_est+3, velocityError, earthScMoonAngle);
        
        // Send Solution Message for Pic 1
        if (scComms != nullptr) {
            std::cout << "Sending Solution Message to ScComms" << std::endl;
            scComms -> sendMessage(solutionMessage);
            fprintf(logFile, "Sent Message: SolutionMessage to ScComms\n");
        }
        
        
        // Kalman Filter Position for Pic 2
        std::cout << "Start: Kalman Filter Iteration" << std::endl;
        
        // Find and Log Reference Trajectory for Pic 2
        get_Reference_Trajectory(X_ref, ref_traj, procMessage->timeStamp);
        fprintf(logFile, "ComputeSolution: Earth Ranging: Reference Trajectory: [0] %f, [1] %f, [2] %f, [3] %f, [4] %f, [5] %f \n", X_ref[0], X_ref[1], X_ref[2], X_ref[3], X_ref[4], X_ref[5]);
        
        // Call Kalman Filter for Pic 2
        Kalman_Filter_Iteration(x_hat, phi, P, r_E_SC2, R, X_ref, dataMessage->satTime, X_est);
        std::cout << "End: Kalman Filter Iteration" << std::endl;
        
        // Find and Log E/Sc/M angle for Pic 2
        earthScMoonAngle = Earth_SC_Moon_Angle(X_est, dataMessage->ephem);
        fprintf(logFile, "Compute Solution: Earth Spacecraft Moon Angle = %f\n", earthScMoonAngle);
        
        // Find State Error and Log it for Pic 2
        State_Error(X_ref, X_est, positionError, velocityError);
        fprintf(logFile, "Compute Solution: Position Error: X_e = %f, Y_e = %f, Z_e = %f\n Velocity Error: VX_e = %f, VY_e = %f, VZ_e = %f\n", positionError[0], positionError[1], positionError[2], velocityError[0], velocityError[1], velocityError[2]);
        
        // Update Solution Message
        std::cout << "Updateing SOlution Message" << std::endl;
        solutionMessage->update(X_est, positionError, X_est+3, velocityError, earthScMoonAngle);
        
        // Update Stored Velocity and Stored Position
        memcpy(velSC, X_est+3, 3* sizeof(double));
        memcpy(r_E_SC, X_est, 3*sizeof(double));
        
        std::cout << "Sending Solution Message" << std::endl;
        if (scComms != nullptr) {
            scComms -> sendMessage(solutionMessage);
            fprintf(logFile, "Sent Message: SolutionMessage to ScComms\n");
        }
        
        firstImage = true;
        
    } else {
        // Moon Ranging to find Position
        fprintf(logFile, "ComputeSolution: Moon Ranging\n");
        
        // Set Pointing
        point = PEM_Moon;
        
        // find and log reference trajectory
        get_Reference_Trajectory(X_ref, ref_traj, procMessage->timeStamp);
        fprintf(logFile, "ComputeSolution: Earth Ranging: Reference Trajectory: [0] %f, [1] %f, [2] %f, [3] %f, [4] %f, [5] %f \n", X_ref[0], X_ref[1], X_ref[2], X_ref[3], X_ref[4], X_ref[5]);
        
        // Log Inputs to Moon Range
        fprintf(logFile, "ComputeSolution: Moon Ranging: INPUTS: quat: [%f, %f, %f, %f],\n alpha = %f, beta = %f, theta = %f \n", dataMessage->quat[0], dataMessage->quat[1], dataMessage->quat[2], dataMessage->quat[3], procMessage->alpha, procMessage->beta, procMessage->theta);
        
        // Get Moon Range
        std::cout << "Start: MOON RANGING: Position From Moon Range" << std::endl;
        Position_From_Moon_Range(dataMessage->ephem, dataMessage->quat, procMessage->alpha, procMessage->alpha, procMessage->theta, r_E_SC);
        std::cout << "End: MOON RANGING: Position From Moon Range" << std::endl;
        
        // Log Outputs for Moon Range
        fprintf(logFile, "ComputeSolution: Earth Range: Kalman Filter Call Inputs: Time: %ld R[0] = %f, R[1] = %f, R[2] = %f\n", dataMessage->satTime, r_E_SC[0], r_E_SC[1], r_E_SC[2]);
        
        // Start Kalman Filter Call
        std::cout << "Start: Kalman Filter Iteration" << std::endl;
        Kalman_Filter_Iteration(x_hat, phi, P, r_E_SC, R, X_ref, dataMessage->satTime, X_est);
        fprintf(logFile, "ComputeSolution: Moon Range: Kalman Filter Results: X =  %f Y = %f, Z = %f, V_X =  %f V_Y = %f, V_Z = %f,\n", X_est[0], X_est[1], X_est[2], X_est[3], X_est[4], X_est[5]);
        std::cout << "End: Kalman Filter Iteration" << std::endl;
        
        // Find E/Sc/M angle and log it
        earthScMoonAngle = Earth_SC_Moon_Angle(X_est, dataMessage->ephem);
        fprintf(logFile, "Compute Solution: Earth Spacecraft Moon Angle = %f\n", earthScMoonAngle);
        
        // Find State Error and Log it
        State_Error(X_ref, X_est, positionError, velocityError);
        fprintf(logFile, "Compute Solution: Position Error: X_e = %f, Y_e = %f, Z_e = %f\n Velocity Error: VX_e = %f, VY_e = %f, VZ_e = %f\n", positionError[0], positionError[1], positionError[2], velocityError[0], velocityError[1], velocityError[2]);
        
        // Update Solution Message
        std::cout << "Updateing SOlution Message" << std::endl;
        solutionMessage->update(X_est, positionError, X_est+3, velocityError, earthScMoonAngle);
        
        // Update Stored Velocity and Stored Position
        memcpy(velSC, X_est+3, 3* sizeof(double));
        memcpy(r_E_SC, X_est, 3*sizeof(double));
        
        std::cout << "Sending Solution Message" << std::endl;
        if (scComms != nullptr) {
            scComms -> sendMessage(solutionMessage);
            fprintf(logFile, "Sent Message: SolutionMessage to ScComms\n");
        }
    }
    
    std::cout << "Ending Compute Solution" << std::endl;
}

//
// Spacecraft-Moon position vector
// Arguments    : const double r_E_SC[3]
//                const double r_E_M[3]
// Return Type  : double
//

double GNC::norm(double* vec) {
    fprintf(logFile, "ComputeSolution: Norm Function\n");
    return sqrt(pow(vec[0],2) + pow(vec[1],2) + pow(vec[2],2));
}

double GNC::Earth_SC_Moon_Angle(const double r_E_SC[3], const double r_E_M[3]) {
    std::cout << "Starting SC Earth Moon ANgle" << std::endl;
    fprintf(logFile, "Earth_SC_Moon_Angle: Entered Earth_SC_Moon_Angle Function\n");
    double c;
    double r_SC_M[3];
    double r_SC_E[3];
    int k;
    double b_r_SC_M;
    
    //  Earth-Spacecraft-Moon Angle Function
    //   Calculates the Earth-Spacecraft-Moon angle from the spacecraft and moon
    //   positions.
    //
    //   Author:   Cameron Maywood
    //   Created:  3/8/2017
    //   Modified: 3/8/2017
    //             _____________________________________________________________
    //   Inputs:  |          r_E_SC         |  Spacecraft ECI position vector   |
    //            |          r_E_M          |  Moon ECI position vector         |
    //            |_________________________|___________________________________|
    //   Outputs: |   angle_Earth_SC_Moon   |   Earth-spacecraft-moon angle     |
    //            |_________________________|___________________________________|
    //  Spacecraft-Earth position vector
    //  Earth-spacecraft-Moon angle
    c = 0.0;
    for (k = 0; k < 3; k++) {
        b_r_SC_M = r_E_M[k] - r_E_SC[k];
        c += b_r_SC_M * -r_E_SC[k];
        r_SC_M[k] = b_r_SC_M;
        r_SC_E[k] = -r_E_SC[k];
    }
    
    fprintf(logFile, "Earth_SC_Moon_Angle: Leaving Earth_SC_Moon_Angle Function\n");
    std::cout << "Starting SC Earth Moon ANgle" << std::endl;
    return 57.295779513082323 * std::acos(c / (norm(r_SC_M) * norm(r_SC_E)));
}


//
// Arguments    : const double X_ref[6]
//                const double X_est[6]
//                double posError[3]
//                double velError[3]
// Return Type  : void
//
void GNC::State_Error(const double X_ref[6], const double X_est[6], double posError[3], double velError[3]) {
    int i;
    std::cout << "Starting State Error" << std::endl;
    fprintf(logFile, "State_Error: Entered State_Error Function\n");
    
    //  State Error Function
    //   Calculates the state error given the reference state and estimated
    //   state.
    //
    //   Author:   Cameron Maywood
    //   Created:  3/9/2017
    //   Modified: 3/9/2017
    //             _________________________________________________
    //   Inputs:  |     X_ref     |   Spacecraft reference state.   |
    //            |     X_est     |   Spacecraft estimated state.   |
    //            |_______________|_________________________________|
    //   Outputs: |    posError   |   Spacecraft position error.    |
    //            |    velError   |   Spacecraft velocity error.    |
    //            |_______________|_________________________________|
    for (i = 0; i < 3; i++) {
        posError[i] = X_ref[i] - X_est[i];
        velError[i] = X_ref[i + 3] - X_est[i + 3];
    }
    fprintf(logFile, "State_Error: Leaving State_Error Function\n");
    std::cout << "Ending State Error" << std::endl;
}

void GNC::read_ConfigFile(std::string config_file) {
    std::ifstream file(config_file);
    
    if (!file) {
        fprintf(logFile, "Error: read Config File File %s could not be opened for reading\n", config_file.c_str());
        throw "File Could Not Be Opened for Reading";
    }
    
    std::string line;
    std::getline(file, line);
    file >> r_E_SC[0];
    file >> r_E_SC[1];
    file >> r_E_SC[2];
    
    std::getline(file, line);
    std::getline(file, line);
    file >> range_EarthRangeCutoff;
    
    std::getline(file, line);
    std::getline(file, line);
    file >> range_AnglesCutoff;
}
// Read Reference Trajectory
void GNC::read_referencTraj(std::string ref_trajectory_file) {
    std::ifstream file(ref_trajectory_file);
    
    if (!file) {
        fprintf(logFile, "Error: ReadReference Trajectory: File %s could not be opened for reading\n", ref_trajectory_file.c_str());
        throw "File Could Not Be Opened for Reading";
    }
    
    double number;
    int column = 1;
    
    while(file){
        file >> number;
        if(column == 1){
            ref_traj.time.push_back(number);
            ++column;
        }
        else if(column == 2){
            ref_traj.X.push_back(number);
            ++column;
        }
        else if(column == 3){
            ref_traj.Y.push_back(number);
            ++column;
        }
        else if(column == 4){
            ref_traj.Z.push_back(number);
            ++column;
        }
        else if(column == 5){
            ref_traj.VX.push_back(number);
            ++column;
        }
        else if(column == 6){
            ref_traj.VY.push_back(number);
            ++column;
        }
        else{
            ref_traj.VZ.push_back(number);
            column = 1;
        }
    }
}



// *******************************
//
// Message Handlers: Supported by GNC
//
// ********************************

/*
 Send Status to WatchDog
 */
void GNC::handleProcessHealthAndStatusRequest(ProcessHealthAndStatusRequest* msg, ServiceInternal* service) {
    fprintf(logFile, "Received Message: ProcessHealthAndStatusRequest from WatchDog\n");
    msg->print(logFile);
    
    // Update ProcessHealthAndStatusResponse Message
    processHealthMessage->update(localError);
    
    // Send Status Message
    service->sendMessage(processHealthMessage);
    fprintf(logFile, "Sent Message: StatusAndHealthResponse to WatchDog\n");
    
    // Reset Error Enum
    localError = PE_AllHealthy;
}

/*
 1. Store the Data Message in circular buffer
 */
void GNC::handleDataMessage(DataMessage* msg, ServiceInternal* service) {
    fprintf(logFile, "Received Message: DataMessage from ScComms\n");
    //msg->print(logFile);
    
    // Put Data Into Circular Buffer
    circBuf.put(msg);
}

/*
 1. Get Spacecraft Data Message
 2. Call Compute
 3. Send a Solution Message
 */
void GNC::handleProcessedImageMessage(ProcessedImageMessage* msg, ServiceInternal* service) {
    fprintf(logFile, "Received Message: ProcessedImageMessage from ScComms\n");
    msg->print(logFile);
    
    std::cout << "STARTING: Handle Processed Image Message" << std::endl;
    DataMessage* scData;
    std::cout << "Attempting to Find Data Message" << std::endl;
    
    try {
        scData = circBuf.get(msg->timeStamp);
        fprintf(logFile, "HandleProcessedImageMessage: Corresponding Data Message Found\n");
    } catch (const char* exception) {
        fprintf(logFile, "Error: Data Message Not Received, Exception: %s\n", exception);
        return;
    }
    
    std::cout << "Attempting to Compute Solution" << std::endl;
    // Compute Solution and Update Solution Message
    try {
        fprintf(logFile, "HandleProcessedImageMessage: Calling Compute Solution\n");
        computeSolution(scData, msg);
        
    } catch (InvalidInputs &e) {
        fprintf(logFile, "Error: HandleProcessedImageMessage() InvalidInputs Exception Caught: %s\n", e.what());
        localError = PE_InvalidInputs;
        
    } catch (InvalidOutput &e) {
        fprintf(logFile, "Error: HandleProcessedImageMessage() InvalidOutput Exception Caught: %s\n", e.what());
        localError = PE_InvalidOutput;
        
    } catch(std::exception &exception) {
        fprintf(logFile, "Error: HandleProcessedImageMessage() Exception Caught: %s\n", exception.what());
        localError = PE_NotHealthy;
        throw;
        
    } catch (...) {
        fprintf(logFile, "Error: HandleProcessedImageMessage() Unknown Type of Exception Caught\n");
        throw;
    }
    
    std::cout << "Attempting to Send Solution Message" << std::endl;
    // Send Solution Message
    std::cout << "ENDING: Handle Processed Image Message" << std::endl;
}


// *******************************
//
// Message Handlers: Not Supported by GNC
//
// ********************************
void GNC::handleOSPREStatus(OSPREStatus* msg, ServiceInternal* service) {
    fprintf(logFile, "Error: Invalid Message Recived: OSPRE Status, Closing Connection\n");
    service->closeConnection();
}
void GNC::handleProcessHealthAndStatusResponse(ProcessHealthAndStatusResponse* msg, ServiceInternal* service) {
    fprintf(logFile, "Error: Invalid Message Recived: ResponseMessage, Closing Connection\n");
    service->closeConnection();
}
void GNC::handleCaptureImageRequest(CaptureImageRequest* msg, ServiceInternal* service) {
    fprintf(logFile, "Error: Invalid Message Recived: CaptureImageRequest, Closing Connection\n");
    service->closeConnection();
}
void GNC::handleImageAdjustment(ImageAdjustment* msg, ServiceInternal* service) {
    fprintf(logFile, "Error: Invalid Message Recived: ImageAdjustmentMessage, Closing Connection\n");
    service->closeConnection();
}
void GNC::handleImageMessage(ImageMessage* msg, ServiceInternal* service) {
    fprintf(logFile, "Error: Invalid Message Recived: ImageMessage, Closing Connection\n");
    service->closeConnection();
}
void GNC::handlePointingRequest(PointingRequest* msg, ServiceInternal* service) {
    fprintf(logFile, "Error: Invalid Message Recived: PointingRequest, Closing Connection\n");
    service->closeConnection();
}
void GNC::handleSolutionMessage(SolutionMessage* msg, ServiceInternal* service){
    fprintf(logFile, "Error: Invalid Message Recived: SolutionMessage, Closing Connection\n");
    service->closeConnection();
}

