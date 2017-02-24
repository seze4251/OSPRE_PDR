//
//  Server.h
//  Server
//
//  Created by Seth on 11/10/2016.
//  Copyright © 2016 Seth. All rights reserved.
//


#include <iostream>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "Server.h"

Server* Server::appl;

Server::Server() {
    signal(SIGPIPE, SIG_IGN);
    t_val.tv_sec = 5;
    t_val.tv_usec = 0;
}

Server::~Server() {
    
}

Selector& Server::getSelector() {
    return sel;
}

Server* Server::getAppl() {
    return appl;
}

void Server::setAppl(Server* srv) {
    appl = srv;
}

void Server::handleTimeout() {
    std::cout << "Server::handleTimeout() I should do something here" << std::endl;
}

void Server::setTimeoutTime(int sec, int micro) {
    t_val.tv_sec = sec;
    t_val.tv_usec = micro;
}

int Server::run() {
    bool terminateFlag = false;
    timeval tempTimeVal;
    
    while (terminateFlag == false) {
        tempTimeVal = t_val;
        if (sel.select(&tempTimeVal) == -1) {
            throw "Server::run() select() error";
            break;
        }
        
        handleTimeout();
    }
    
    return 0;
}


