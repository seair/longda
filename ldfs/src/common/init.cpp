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

#include "util.h"
#include "datetime.h"
#include "log.h"

#include "threadpool.h"
#include "sedaconfig.h"
#include "stagefactory.h"

#include "killthread.h"
#include "timerstage.h"
#include "sedastatsstage.h"

// give seed for random number generation
// TODO: the implementation of rand() in glibc is thread-safe,
//       but it will take a global lock to protect static data structure.
//       could consider using XrandNN_r() later
void _seedRandom()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
}

bool*&
_getInit()
{
    static bool  util_init   = false;
    static bool* util_init_p = &util_init;
    return util_init_p;
}

bool
UtilGetInit()
{
    return *_getInit();
}

void
UtilSetInit(bool value)
{
    *_getInit() = value;
    return ;
}

int SedaInit(ConfigPath *pConfigFiles)
{
    //Initialize the static data structures of threadpool
    Threadpool::initThreadPool();

    // try to parse the seda configuration files
    SedaConfig*          config = SedaConfig::getInstance();
    SedaConfig::status_t configStat;
    
    configStat = config->parse();
    if (configStat != SedaConfig::SUCCESS) {
        LOG_ERROR("Error: unable to parse file %s", pConfigFiles->mProperties.c_str());
        return STATUS_INIT_SEDA;
    }

    // Log a message to indicate that we are restarting, when looking
    // at a log we can see if mmon is restarting us because we keep 
    // crashing.
    LOG_INFO("(Re)Starting State: Pid: %u Time: %s", 
                    (unsigned int)getpid(), DateTime::now().toStringLocal().c_str());
    LOG_INFO("The process Name is %s", pConfigFiles->mProcessName.c_str());

    // try to initialize the seda configuration
    configStat = config->init();
    if (configStat != SedaConfig::SUCCESS) {
        LOG_ERROR("SedaConfig: unable to initialize seda stages");
        return STATUS_INIT_SEDA;
    }

    theSedaConfig() = config;

    return STATUS_SUCCESS;
}

int
UtilInit(ConfigPath *pConfigFiles) 
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&init_mutex);
    if (UtilGetInit())
    {
        pthread_mutex_unlock(&init_mutex);
        return STATUS_SUCCESS;
    }
    UtilSetInit(true);

    // Initialize global variables before enter multi-thread mode
    // to avoid race condition
    theSwVersion();

    // initialize class factory instances here    
    static StageFactory KillThreadFactory(
                            "KillThreads", &KillThreadStage::makeStage);
    static StageFactory TimerFactory(
                            "TimerStage", &TimerStage::makeStage);
    static StageFactory SedaStatsFactory(
                            "SedaStatsStage", &SedaStatsStage::makeStage);

    //Read Configuration files
    int rc = theGlobalProperties()->ReadCfgFileEx(pConfigFiles->mProperties);
    if (rc) 
    {
        std::cerr << "Failed to read configuration files" << std::endl;
        return STATUS_PROPERTY_ERR;
    }

    // Init tracer
    rc = InitLog(pConfigFiles->mProcessName, theGlobalProperties() );
    if(rc) {
        std::cerr << "Failed to init Log" << std::endl;
        return STATUS_INIT_LOG;
    }

    rc = SedaInit(pConfigFiles);
    if (rc)
    {
        LOG_ERROR("Failed to init seda");
        return STATUS_INIT_SEDA;
    }

    _seedRandom();
    pthread_mutex_unlock(&init_mutex);

    LOG_INFO("Successfully init utility");

    return STATUS_SUCCESS;
}

void *&
GetHandler()
{
    static void *_eventHandler = 0;
    return _eventHandler;
}

void
UtilFinalize()
{

    if (NULL != theGlobalProperties()){
        delete theGlobalProperties();
        theGlobalProperties() = NULL;
    }

    SedaConfig *sedaConfig = SedaConfig::getInstance();
    delete sedaConfig;
    SedaConfig::getInstance() = NULL;


    LOG_ERROR("Shutdown Cleanly. Pid: %d Time: %s",
                  (int)getpid(), DateTime::now().toStringLocal().c_str());

    // Finalize tracer
    CleanupLog();

    UtilSetInit(false);
    return;
}


CLog *gLog = NULL;

int 
InitLog(const std::string &procName, CCfgReader *procProperties)
{
    try
    {
        // we had better alloc one lock to do so, but simplify the logic
        if ( gLog )
        {
            return STATUS_SUCCESS;
        }
        const std::string logFileKey("LOG_FILE_NAME");
        std::string       logFile;
        logFile = procProperties->GetParamValue( logFileKey, DEFAULT_SESSION);
        if (logFile.size() == 0)
        {
            std::cerr << __FUNCTION__ 
                  << ":"
                  << __LINE__ 
                  <<"Not set the log file name in configure file for "
                  << procName
                  << std::endl;
            return STATUS_INIT_LOG;
        }
        if (logFile[0] != '/')
        {
            char *dirName = get_current_dir_name();
            if (dirName)
            {
                logFile = dirName + std::string("/") + logFile;
                free(dirName);
            }
        }

        LOG_LEVEL  logLevel      = LOG_LEVEL_INFO;
        const std::string logKey("LOG_FILE_LEVEL");
        std::string       logValue;
        logValue = procProperties->GetParamValue( logKey, DEFAULT_SESSION);
        if (logValue.size())
        {
            int log = (int)logLevel;
            Xlate::strToVal(logValue, log);
            logLevel = (LOG_LEVEL)log;
        }
        
        LOG_LEVEL  consoleLevel = LOG_LEVEL_WARN;
        const std::string consoleKey("LOG_CONSOLE_LEVEL");
        std::string       consoleValue;
        consoleValue = procProperties->GetParamValue( consoleKey, DEFAULT_SESSION);
        if (consoleValue.size() )
        {
            int console = (int)consoleLevel;
            Xlate::strToVal(consoleValue, console);
            consoleLevel = (LOG_LEVEL)console;
        }

        gLog = new CLog(logFile, logLevel, consoleLevel);
        if (gLog == NULL)
        {
            std::cerr << __FUNCTION__ 
                  << ":"
                  << __LINE__ 
                  <<"Failed to construct the log "
                  << logFile
                  << " for "
                  << procName
                  << std::endl;
            return STATUS_INIT_LOG;
        }

        const std::string dlmKey("DefaultLogModules");
        std::string       dlmValue;
        dlmValue = procProperties->GetParamValue( dlmKey, DEFAULT_SESSION);
        if (dlmValue.size() )
        {
            gLog->SetDefaultModule(dlmValue);
        }

        if (GlobalConfigPath()->mDemon)
        {
            SysLogRedirect(logFile.c_str(), logFile.c_str());
        }

        return STATUS_SUCCESS;
    }
    catch (std::exception &e)
    {
        std::cerr << __FUNCTION__ 
                  << ":"
                  << __LINE__ 
                  <<"Failed to init log for "
                  << procName
                  << std::endl;
        return STATUS_INIT_LOG;
    }

    return STATUS_SUCCESS;
}

void
CleanupLog()
{

    if (gLog)
    {
        delete gLog;
        gLog = NULL;
    }
    return ;
}

void WaitForSignals(sigset_t *signal_set, int& sig_number)
{
    while (true) {
        errno = 0;
        int ret = sigwait(signal_set, &sig_number);
        LOG_DEBUG("sigwait return value: %d \n", ret);
        if (ret == 0) {
            LOG_DEBUG("signal caught: %d\n", sig_number);
            break;
        } else {
            char errstr[256];
            strerror_r(errno, errstr, sizeof(errstr));
            LOG_ERROR("error (%d) %s\n", errno, errstr);
        }
    }
}

void WaitForSignals(sigset_t *signal_set)
{
    int sig_number;
    WaitForSignals(signal_set,sig_number);
}

//! Global config files path
ConfigPath*& GlobalConfigPath()
{
    static ConfigPath* configPath = new ConfigPath();

    return configPath;
}

//! Accessor function which wraps global properties object
CCfgReader*& 
theGlobalProperties(){
    static CCfgReader   *globalCfgReader = new CCfgReader();
    return globalCfgReader;
}

