// __CR__
// Copyright (c) 2008-2011 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__


/*
 * author: hustjackie@Longda.com
 * date:   2011/04/01
 * func:   Provide report enter entry
 */ 
 
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <string>
#include <iostream>

#include "util.h"
#include "pidfile.h"
#include "log.h"

#include "afuseinit.h"
#include "fusentry.h"

extern int xmp_fusentry();

int main(int argc, char** argv)
{
    // Initializations

    std::string processName = GetProcessName(argv[0]);

    GlobalConfigPath()->Init(processName);
    
    // Process args
    int rc = STATUS_SUCCESS;
    int opt;
    extern char* optarg;
    while((opt = getopt(argc, argv, "ds:f:o:e:h")) > 0) {
        switch(opt) {
        case 'f':
            GlobalConfigPath()->mProperties = optarg;
            break;
        case 'o':
            GlobalConfigPath()->mStdOut     = optarg;
            break;
        case 'e':
            GlobalConfigPath()->mStdErr     = optarg;
            break;
        case 'd':
            GlobalConfigPath()->mDemon = true;
            break;
        case 'h':
        default:
            Usage();
            return STATUS_INVALID_PARAM;
        }
    }

    rc = Init();
    if (rc)
    {
        std::cerr << "Failed to init process" << std::endl;
        Finalize();
        return STATUS_FAILED_INIT;
    }

    if (gADFS)
    {
        LOG_INFO("Go ADFS");
        CFuse::FuseEntry();
        
    }
    else
    {
        LOG_INFO("Go local Filesystem, not ADFS");
        xmp_fusentry();
    }
    
    return STATUS_SUCCESS;
}


