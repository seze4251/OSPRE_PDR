//
//  Service.cpp
//  Service
//
//  Created by Seth on 11/24/2016.
//  Copyright © 2016 Seth. All rights reserved.
//

#include "Service.h"
#include <iostream>
#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>

Service::Service(Selector &sharedSel) : sel(sharedSel) {
    std::cout << "Service Constructor \n";
}

int Service::openServerSocket(int portNumber) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // Stream Socket
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    int s = ::getaddrinfo(NULL, (char *)(&portNumber), &hints, &result);
    
    if (s != 0) {
        std::cout << "Open Server Socket Failed \n";
        return -1;
    }
    
    // Bind Server Socket
    int sfd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // printf("family %d, socketType %d, protocol %d\n", rp->ai_family,
        //       rp->ai_socktype, rp->ai_protocol);
        
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;
        
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;                  /* Success */
        }
        close(sfd);
    }
    
    // Check to make sure bind to address succeeded
    if (sfd == -1) {
        std::cout << "Open Server Socket Failed \n";
        return -1;
    }
    
    freeaddrinfo(result);
    
    // Listen on Server Socket for incomming connections
    if (listen(sfd, 4) == -1) {
        std::cout << "Open Server Socket Failed \n";
        return -1;
    }
    
    return sfd;
}

int Service::connectToServer(char *serverHosts, int serverPort) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    
    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; // TCP Stream Socket
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */
    
    
    int s = ::getaddrinfo(serverHosts, (char *) &serverPort, &hints, &result);
    
    if (s != 0) {
        return E_GETADDRINFO;
    }
    
    int fd = -1;
    
    while (1) {
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            
            // Check to see if socket opens
            if (fd == -1) {
                continue;
            }
            
            // Attempt to connect socket
            if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
                break;
            }
            
            close(fd);
            fd = -1;
        }
        
        if (fd != -1) {
            break;
        }
        
        fprintf(stdout, "failed to connect, sleeping for 10, will try again\n");
        sleep(10);
    }
    
    if (fd == -1) {
        return E_CONNECTION_FAILURE;
    }
    
    freeaddrinfo(result);
    return fd;
}
