// __CR__
// Copyright (c) 2008-2010 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "util.h"
#include "log.h"

#include "sedaconfig.h"
#include "stagefactory.h"

SedaConfig* SedaConfig::instance = NULL;

#define SEDA_CONFIG_SESSION         "Seda_Config"
#define THREAD_POOLS_KEY            "ThreadPools"

#define STAGE_NAMES                 "STAGES"
#define STAGE_THREADID              "ThreadId"
#define STAGE_NEXT                  "NextStages"


SedaConfig*&
SedaConfig::getInstance()
{
    if (instance == NULL)
    {
        instance = new SedaConfig();
        ASSERT((instance != NULL), "failed to allocate SedaConfig");
    }
    return instance;
}

//! Constructor
SedaConfig::SedaConfig() 
  : mCfgFile(),
    mCfgStr(),
    mThreadPools(),
    mStages()      
{
    return;
}


//! Destructor
SedaConfig::~SedaConfig() 
{
    ASSERT(instance, "Instance should not be null");
    // check to see if clean-up is necessary
    if ((!mThreadPools.empty()) || (!mStages.empty())) {
        cleanup();
    }

    instance = NULL;
}


//! Set the file holding the configuration
void 
SedaConfig::setCfgFilename(const char* filename)
{
    mCfgStr.clear();
    mCfgFile.clear();
    if (filename != NULL) { mCfgFile.assign(filename); }
    return;
}


//! Set the string holding the configuration
void 
SedaConfig::setCfgString(const char* configStr)
{
    mCfgStr.clear();
    mCfgFile.clear();
    if (configStr != NULL) { mCfgStr.assign(configStr); }
    return;
}


//! Parse config file or string
SedaConfig::status_t
SedaConfig::parse()
{
    // first parse the config
    try
    {
        //skip parse in this implementation
        // all configuration will be put into one file
    }
    catch (const std::exception& e)
    {
        std::string errMsg(e.what());
        LOG_ERROR( "Seda config parse failed w/ error %s", errMsg.c_str());
        return PARSEFAIL;
    }
    LOG_DEBUG("Seda config parse success\n");

    return SUCCESS;
}

//! instantiate the parsed SEDA configuration
SedaConfig::status_t 
SedaConfig::instantiateCfg()
{
    status_t stat = SUCCESS;

    // instantiate the configuration
    stat = instantiate();
    
    return stat;
}


//! start the configuration - puts the mStages into action
SedaConfig::status_t 
SedaConfig::start()
{
    status_t stat = SUCCESS;

    ASSERT(mThreadPools.size(), "Configuration not yet instantiated");

    // start the mStages one by one.  connect() calls 
    std::map<std::string, Stage*>::iterator iter = mStages.begin();
    std::map<std::string, Stage*>::iterator end  = mStages.end();

    while (iter != end) {
        if (iter->second != NULL) {
            Stage* stg = iter->second;
            bool ret = stg->connect();
            if (!ret) {
                cleanup();
                stat = INITFAIL;
                break;
            }
        }
        iter++;
    }

    return stat;
}



//! Initialize the mThreadPools and mStages
SedaConfig::status_t
SedaConfig::init()
{
    status_t stat = SUCCESS;

    // check the preconditions
    ASSERT(mStages.empty(), "Attempt to initialize sedaconfig twice");
    ASSERT(mThreadPools.empty(),  
           "Attempt to initialize sedaconfig twice");
    
    // instantiate the parsed config
    if (stat == SUCCESS) {
        stat = instantiate();
    }
    
    // start it running
    if (stat == SUCCESS) {
        stat = start();
    }

    return stat;
}


//! Clean-up the threadpool and mStages
void
SedaConfig::cleanup()
{
    // first disconnect all mStages
    if (mStages.empty() == false) {
        std::map<std::string, Stage*>::iterator iter = mStages.begin();
        std::map<std::string, Stage*>::iterator end  = mStages.end();
        while (iter != end) {
            if (iter->second != NULL) {
                Stage* stg = iter->second;
                if (stg->isConnected()) {
                    stg->disconnect();
                }                
            }
            iter++;
        }
    }
    LOG_TRACE("mStages disconnected\n");

    // now delete all mStages and mThreadPools
    clearconfig();
}

void
SedaConfig::initEventHistory()
{
    //check whether event histories are enabled
    std::string evHistStr;
    bool evHist = false;
    evHistStr = theGlobalProperties()->GetParamValue("EventHistory", SEDA_CONFIG_SESSION);
    if ( evHistStr.compare("true") == 0) {
        evHist = true;
    }
    theEventHistoryFlag() = evHist;

    //set max event hops
    std::string maxEventHopsStr;
    u32_t maxEventHops = 100;
    maxEventHopsStr = theGlobalProperties()->GetParamValue("MaxEventHops", SEDA_CONFIG_SESSION);
    if (maxEventHopsStr.empty() == false) {
        Xlate::strToVal(maxEventHopsStr, maxEventHops);
    }
    theMaxEventHops() = maxEventHops;

    return ;
}

SedaConfig::status_t
SedaConfig::initThreadPool()
{
    try
    {
        std::string names = theGlobalProperties()->GetParamValue("NAME", THREAD_POOLS_KEY);
        if (names.empty())
        {
            LOG_ERROR("SedaConfig hasn't set threadpools' name");
            return INITFAIL;
        }

        std::vector<std::string> nameList;
        Xlate::SplitString(names, CFG_DELIMIT_TAG, nameList);

        std::string counts = theGlobalProperties()->GetParamValue("COUNT", THREAD_POOLS_KEY);
        if (counts.empty())
        {
            LOG_ERROR("SedaConfig hasn't set threadpools' counts");
            return INITFAIL;
        }

        std::vector<std::string> countList;
        Xlate::SplitString(counts, CFG_DELIMIT_TAG, countList);

        
        for (int pos = 0; pos != nameList.size(); pos++)
        {
            std::string &threadName = nameList[pos];

            std::string &countStr = countList[pos];
            int   threadCount = 1;
            Xlate::strToVal(countStr, threadCount);
            if (threadCount < 1)
            {
                LOG_ERROR( "Wrong SedaConfig file, threadpools %s count is less than 1",
                    threadName.c_str());
                return INITFAIL;
            }

            mThreadPools[threadName] = new Threadpool(threadCount, threadName);
            if (mThreadPools[threadName] == NULL)
            {
                LOG_ERROR( "Failed to new %s threadpool\n", threadName.c_str());
                return INITFAIL;
            }
        }
        
    }
    catch (std::exception &e)
    {
        LOG_ERROR( "Failed to init mThreadPools:%s\n", e.what());
        return INITFAIL;
    }

    int nPools = mThreadPools.size();
    if (nPools < 1) {
        LOG_ERROR( "Invalid number of mThreadPools:%d", nPools);
        clearconfig();
        return INITFAIL;
    }

    return SUCCESS;
}

SedaConfig::status_t
SedaConfig::initStages()
{
    std::string stageNames = theGlobalProperties()->GetParamValue(STAGE_NAMES, SEDA_CONFIG_SESSION);
    if (stageNames.empty())
    {
        LOG_ERROR("Hasn't set stages name in %s", SEDA_CONFIG_SESSION);
        clearconfig();
        return INITFAIL;
    }

    Xlate::SplitString(stageNames, CFG_DELIMIT_TAG, mStageNames);

    for (std::vector<std::string>::iterator it = mStageNames.begin(); it != mStageNames.end(); it++)
    {
        
        std::string stageName(*it);
        Stage *stage = StageFactory::makeInstance(stageName);
        if (stage == NULL) 
        {
            LOG_ERROR( "Failed to make instance of stage %s", stageName.c_str());
            clearconfig();
            return INITFAIL;
        }
        mStages[stageName] = stage;

        std::string threadName = theGlobalProperties()->GetParamValue(STAGE_THREADID, stageName);
        if (threadName.empty())
        {
            LOG_ERROR("Failed to set %s of the %s", STAGE_THREADID, stageName.c_str());
            clearconfig();
            return INITFAIL;
        }
        Threadpool* t = mThreadPools[threadName];        
        stage->setPool(t);

    }//end for stage
    
    if (mStages.size() < 1) {
        LOG_ERROR( "Invalid number of mStages: %zu\n", mStages.size());
        clearconfig();
        return INITFAIL;
    }

    return SUCCESS;
}

SedaConfig::status_t
SedaConfig::genNextStages()
{
    for (std::vector<std::string>::iterator it = mStageNames.begin(); it != mStageNames.end(); it++)
    {
        
        std::string stageName(*it);
        Stage *stage = mStages[stageName];


        std::string nextStageNames = theGlobalProperties()->GetParamValue(STAGE_NEXT, stageName);
        if (nextStageNames.empty())
        {
            continue;
        }

        std::vector<std::string> nextStageNameList;
        Xlate::SplitString(nextStageNames, CFG_DELIMIT_TAG, nextStageNameList);
        for (std::vector<std::string>::iterator nextIt = nextStageNameList.begin();
             nextIt != nextStageNameList.end(); nextIt++)
        {
            std::string &nextStageName = *nextIt;
            Stage       *nextStage     = mStages[nextStageName];
            stage->pushStage(nextStage);
        }
        

    }//end for stage
    return SUCCESS;
}

//! instantiate the mThreadPools and mStages
SedaConfig::status_t
SedaConfig::instantiate()
{

    initEventHistory();

    SedaConfig::status_t status = initThreadPool();
    if (status)
    {
        LOG_ERROR( "Failed to init thread pool\n");
        return status;
    }

    status = initStages();
    if (status)
    {
        LOG_ERROR( "Failed init mStages\n");
        return status;
    }
    

    status = genNextStages();
    if (status)
    {
        LOG_ERROR( "Failed to generate next stage list\n");
        return status;
    }

    return SUCCESS;
}


//! delete all mThreadPools and mStages
void
SedaConfig::clearconfig()
{
    // delete mStages
    std::map<std::string, Stage*>::iterator s_iter = mStages.begin();
    std::map<std::string, Stage*>::iterator s_end  = mStages.end();
    while (s_iter != s_end) {
        if (s_iter->second != NULL) {
            Stage* stg = s_iter->second;
            ASSERT((!stg->isConnected()),
                   "%s%s", "Stage connected in clearconfig ", stg->getName());
            delete stg;
            s_iter->second = NULL;
        }
        s_iter++;
    }
    mStages.clear();
    LOG_DEBUG("mStages deleted\n");

    // delete mThreadPools
    std::map<std::string, Threadpool *>::iterator t_iter = mThreadPools.begin();
    std::map<std::string, Threadpool *>::iterator t_end  = mThreadPools.end();
    while (t_iter != t_end) {
        if (t_iter->second != NULL) {
            delete t_iter->second;
            t_iter->second = NULL;
        }
        t_iter++;
    }
    mThreadPools.clear();
    LOG_TRACE("%s\n", "thread pools released");
}

void
SedaConfig::getStageNames(std::vector<std::string>& names) const
{
    for (std::map<std::string, Stage*>::const_iterator i = mStages.begin();
         i != mStages.end(); ++i)
    {
        names.push_back((*i).first);
    }
}

void
SedaConfig::getStageQueueStatus(std::vector<int>& stats) const
{
    for (std::map<std::string, Stage*>::const_iterator i = mStages.begin();
         i != mStages.end(); ++i)
    {
        Stage* stg = (*i).second;
        stats.push_back(stg->qlen());
    }
}

//! Global seda config object
SedaConfig*& theSedaConfig()
{
    static SedaConfig* sedaConfig = NULL;
    
    return sedaConfig;
}
