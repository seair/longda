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


#ifndef __LINIT_H__
#define __LINIT_H__

// Basic includes
#include <signal.h>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <assert.h>
#include <vector>

#include "defs.h"
#include "conf/ini.h"

class CProcessParam{
public:
    CProcessParam(){}
    void init(std::string processName)
    {
        assert(processName.empty() == false);
        mProcessName = processName;
        if (mStdOut.empty() == true )
        {
            mStdOut = mProcessName + ".out";
        }
        if (mStdErr.empty() == true )
        {
            mStdErr = mProcessName + ".err";
        }
        if (mProperties.empty() == true)
        {
            mProperties= "./etc/" + mProcessName + ".ini";
        }

        mDemon = false;

        return ;
    }
    
public:
    std::string mStdOut;       //! The output file
    std::string mStdErr;       //! The err output file
    std::string mProperties;   //! The properties config file
    std::string mProcessName;  //! The process name
    bool        mDemon;        // whether demon or not
};

/**
 * Global config files path
 */
CProcessParam*& theProcessParam();



/**
 * init utility function
 */
int  initUtil(CProcessParam *pConfigFiles);

/**
 * cleanup utility function, including seda cleanup
 */
void cleanupUtil();

/**
 * start the seda process, do this will trigger all threads
 */
int initSeda(CProcessParam *pProcessCfg);


#endif // __LINIT_H__
