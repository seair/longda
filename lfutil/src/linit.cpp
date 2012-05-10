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

/**
 * @ author: hustjackie@gmail.com
 * @ date:  2010/04/01
 * @ func:  provide project common init functions
 */

#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <map>


#include "os/mutex.h"
#include "trace/log.h"
#include "math/lmath.h"
#include "time/datetime.h"
#include "os/lprocess.h"
#include "lang/lstring.h"

#include "seda/threadpool.h"
#include "seda/sedaconfig.h"
#include "seda/stagefactory.h"

#include "seda/killthread.h"
#include "seda/timerstage.h"
#include "seda/sedastatsstage.h"
#include "comm/commstage.h"



#include "linit.h"


bool*& _getInit()
{
    static bool util_init = false;
    static bool* util_init_p = &util_init;
    return util_init_p;
}

bool getInit()
{
    return *_getInit();
}

void setInit(bool value)
{
    *_getInit() = value;
    return;
}

//! Global process config
CProcessParam*& theProcessParam()
{
    static CProcessParam* processCfg = new CProcessParam();

    return processCfg;
}



CLog *gLog = NULL;
int initLog(CProcessParam *pProcessCfg, CIni &gProperties)
{
    std::string &procName = pProcessCfg->mProcessName;
    try
    {
        // we had better alloc one lock to do so, but simplify the logic
        if (gLog)
        {
            return 0;
        }

        const std::string logSectionName = "LOG";
        std::map<std::string, std::string> logSection = gProperties.get(logSectionName);

        std::string key;
        std::map<std::string, std::string>::iterator it;

        std::string logFileName;

        // get log file name
        key = "LOG_FILE_NAME";
        it  = logSection.find(key);
        if (it == logSection.end())
        {
            logFileName = procName + ".log";
            std::cout << "Not set log file name, use default " << logFileName << std::endl;
        }
        else
        {
            logFileName = it->second;
        }

        if (logFileName[0] != '/')
        {
            char *dirName = get_current_dir_name();
            if (dirName)
            {
                logFileName = dirName + std::string("/") + logFileName;
                free(dirName);
            }
        }

        LOG_LEVEL logLevel = LOG_LEVEL_INFO;
        key = ("LOG_FILE_LEVEL");
        it  = logSection.find(key);
        if (it != logSection.end())
        {
            int log = (int) logLevel;
            CLstring::strToVal(it->second, log);
            logLevel = (LOG_LEVEL) log;
        }

        LOG_LEVEL consoleLevel = LOG_LEVEL_INFO;
        key = ("LOG_CONSOLE_LEVEL");
        it = logSection.find(key);
        if (it != logSection.end())
        {
            int log = (int) consoleLevel;
            CLstring::strToVal(it->second, log);
            consoleLevel = (LOG_LEVEL) log;
        }


        gLog = new CLog(logFileName, logLevel, consoleLevel);
        if (gLog == NULL)
        {
            std::cerr << SYS_OUTPUT_FILE_POS
                      <<"Failed to construct the log " << logFileName << " for " << procName
                      << std::endl;
            return -1;
        }

        key = ("DefaultLogModules");
        it = logSection.find(key);
        if (it != logSection.end())
        {
            gLog->SetDefaultModule(it->second);
        }

        if (pProcessCfg->mDemon)
        {
            sysLogRedirect(logFileName.c_str(), logFileName.c_str());
        }

        return 0;
    } catch (std::exception &e)
    {
        std::cerr <<"Failed to init log for " << procName
                << SYS_OUTPUT_FILE_POS << SYS_OUTPUT_ERROR
                << std::endl;
        return errno;
    }

    return 0;
}

void cleanupLog()
{

    if (gLog)
    {
        delete gLog;
        gLog = NULL;
    }
    return;
}


int initSeda(CProcessParam *pProcessCfg)
{
    //Initialize the static data structures of threadpool
    Threadpool::createPoolKey();

    // try to parse the seda configuration files
    SedaConfig* config = SedaConfig::getInstance();
    SedaConfig::status_t configStat;

    configStat = config->parse();
    if (configStat != SedaConfig::SUCCESS)
    {
        LOG_ERROR("Error: unable to parse file %s",
                pProcessCfg->mProperties.c_str());
        return errno;
    }

    // Log a message to indicate that we are restarting, when looking
    // at a log we can see if mmon is restarting us because we keep 
    // crashing.
    LOG_INFO("(Re)Starting State: Pid: %u Time: %s",
            (unsigned int)getpid(), DateTime::now().toStringLocal().c_str());
    LOG_INFO("The process Name is %s", pProcessCfg->mProcessName.c_str());

    // try to initialize the seda configuration
    configStat = config->init();
    if (configStat != SedaConfig::SUCCESS)
    {
        LOG_ERROR("SedaConfig: unable to initialize seda stages");
        return errno;
    }

    theSedaConfig() = config;

    return 0;
}

int  initUtil(CProcessParam *pProcessCfg)
{

    if (getInit())
    {

        return 0;
    }

    setInit(true);

    // Initialize global variables before enter multi-thread mode
    // to avoid race condition
    theSwVersion();

    // initialize class factory instances here    
    static StageFactory killThreadFactory("KillThreads", &KillThreadStage::makeStage);
    static StageFactory timerFactory("TimerStage", &TimerStage::makeStage);
    static StageFactory sedaStatsFactory("SedaStatsStage", &SedaStatsStage::makeStage);
    static StageFactory commstageFactory("CommStage", &CommStage::makeStage);

    //Read Configuration files
    int rc = theGlobalProperties()->load(pProcessCfg->mProperties);
    if (rc)
    {
        std::cerr << "Failed to load configuration files" << std::endl;
        return rc;
    }

    // Init tracer
    rc = initLog(pProcessCfg, *theGlobalProperties());
    if (rc)
    {
        std::cerr << "Failed to init Log" << std::endl;
        return rc;
    }


    std::string confData;
    theGlobalProperties()->output(confData);
    LOG_INFO("Output configuration \n%s", confData.c_str());


    seedRandom();

    LOG_INFO("Successfully init utility");

    return STATUS_SUCCESS;
}

void cleanupUtil()
{

    if (NULL != theGlobalProperties())
    {
        delete theGlobalProperties();
        theGlobalProperties() = NULL;
    }

    SedaConfig *sedaConfig = SedaConfig::getInstance();
    delete sedaConfig;
    SedaConfig::getInstance() = NULL;

    LOG_ERROR("Shutdown Cleanly. Pid: %d Time: %s",
            (int)getpid(), DateTime::now().toStringLocal().c_str());

    // Finalize tracer
    cleanupLog();

    setInit(false);
    return;
}

