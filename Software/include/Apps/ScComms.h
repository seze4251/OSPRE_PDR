//
//  ScComms.h
//  ScComms
//
//  Created by Seth on 11/10/2016.
//  Copyright © 2016 Seth. All rights reserved.
//


#ifndef SCCOMMS_H
#define SCCOMMS_H

#include <string>
#include <ctime>
#include <vector>


#include "ServerInternal.h"
#include "WatchDogService.h"
#include "ProcessID.h"
#include "ServiceExternal.h"


class ScComms : public ServerInternal {
public:
    // Constructor
    ScComms(std::string hostName, int localPort, int externalPort);
    
    //Destructor
    ~ScComms();
    
    // Opens ScComms Connections
    virtual void open();
    
    virtual void handleTimeout();
    
    //
    void handleExternalConnection(int fd);
    
    // Message Handlers
    virtual void handleCaptureImageRequest(CaptureImageRequest* msg, ServiceInternal* service);
    virtual void handleDataMessage(DataMessage* msg, ServiceInternal* service);
    virtual void handleImageAdjustment(ImageAdjustment* msg, ServiceInternal* service);
    virtual void handleImageMessage(ImageMessage* msg, ServiceInternal* service);
    virtual void handleOSPREStatus(OSPREStatus* msg, ServiceInternal* service);
    virtual void handlePointingRequest(PointingRequest* msg, ServiceInternal* service);
    virtual void handleProcessHealthAndStatusRequest(ProcessHealthAndStatusRequest* msg, ServiceInternal* service);
    virtual void handleProcessHealthAndStatusResponse(ProcessHealthAndStatusResponse* msg, ServiceInternal* service);
    virtual void handleSolutionMessage(SolutionMessage* msg, ServiceInternal* service);
    virtual void handleProcessedImageMessage(ProcessedImageMessage* msg, ServiceInternal* service);
    
    
private:
    time_t pollTime;
    std::vector<ProcessError> status;
    
    Acceptor external_accept;
    int externalPort;
    ServiceExternal* spacecraft;

};

#endif

// Connect to WatchDog
