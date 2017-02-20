//
//  ProccessHealthAndStatusResponse.h
//  ProccessHealthAndStatusResponse
//
//  Created by Seth on 11/24/2016.
//  Copyright © 2016 Seth. All rights reserved.
//


#ifndef PROCESSHEALTHANDSTATUSRESPONSE_H
#define PROCESSHEALTHANDSTATUSRESPONSE_H

#include <vector>
#include <iostream>

#include "Message.h"
#include "ProcessID.h"
#include "ProcessError.h"

class ProcessHealthAndStatusResponse : public Message {
public:
    //Constructors:
    ProcessHealthAndStatusResponse() : Message(getMessageID(), time(0)) {}
    
    MessageID getMessageID() {return I_ProcessHealthAndStatusResponse; }
    
    //Member Functions
    void update(ProcessError error) {
        this->timeStamp = time(0);
        this->error = error;
    }
    
    void print() {
        printMessageHeader();
        
        printProcessError(error);
        
        if (error == PE_NotHealthy) {
            printProcessError(error);
        }
    }
    
    // Process Health
    ProcessError error;
};

#endif
