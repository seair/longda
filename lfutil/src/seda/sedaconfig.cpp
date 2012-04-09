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
#include <typeinfo>

#include "linit.h"
#include "trace/log.h"
#include "lang/lstring.h"

#include "seda/sedaconfig.h"
#include "seda/stagefactory.h"

SedaConfig* SedaConfig::instance = NULL;

const std::string SedaConfig::SEDA_BASE_NAME = std::string("SEDA_BASE");
const std::string SedaConfig::THREAD_POOLS_NAME = std::string("ThreadPools");


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
    stat = instantiate();
    if (stat) {
        return stat;
    }
    
    // start it running
    stat = start();
    if (stat) {
        return stat;
    }

    return SUCCESS;
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
    std::map<std::string, std::string> baseSection = theGlobalProperties()->get(SEDA_BASE_NAME);
    std::map<std::string, std::string>::iterator it ;
    std::string key;

    //check whether event histories are enabled
    bool evHist = false;
    key = "EventHistory";
    it = baseSection.find(key);
    if (it != baseSection.end())
    {
        if (it->second.compare("true") == 0)
        {
            evHist = true;
        }
    }

    theEventHistoryFlag() = evHist;

    //set max event hops
    u32_t maxEventHops = 100;
    key = "MaxEventHops";
    it = baseSection.find(key);
    if (it != baseSection.end())
    {
        CLstring::strToVal(it->second, maxEventHops);

    }
    theMaxEventHops() = maxEventHops;

    LOG_INFO("Successfully initEventHistory, EventHistory:%d, MaxEventHops:%u",
            (int)evHist, maxEventHops);
    return ;
}

SedaConfig::status_t
SedaConfig::initThreadPool()
{
    try
    {

        std::map<std::string, std::string> baseSection =
                theGlobalProperties()->get(SEDA_BASE_NAME);
        std::map<std::string, std::string>::iterator it;
        std::string key;

        //get thread pool names
        key = THREAD_POOLS_NAME;
        it = baseSection.find(key);
        if (it == baseSection.end())
        {
            LOG_ERROR("Configuration hasn't set %s", key.c_str());
            return INITFAIL;
        }

        std::string poolNames = it->second;
        std::vector<std::string> nameList;
        std::string splitTag ;
        splitTag.assign(1, CIni::CFG_DELIMIT_TAG);
        CLstring::splitString(poolNames, splitTag, nameList);
        
        for (size_t pos = 0; pos != nameList.size(); pos++)
        {
            std::string &threadName = nameList[pos];

            // get count number
            key = "count";
            std::string countStr = theGlobalProperties()->get(key, "4", threadName);

            int   threadCount = 1;
            CLstring::strToVal(countStr, threadCount);
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
        clearconfig();
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
    try
    {
        std::map<std::string, std::string> baseSection =
                theGlobalProperties()->get(SEDA_BASE_NAME);
        std::map<std::string, std::string>::iterator it;
        std::string key;

        // get stage names
        key = "STAGES";
        it = baseSection.find(key);
        if (it == baseSection.end())
        {
            LOG_ERROR("Hasn't set stages name in %s", key.c_str());
            clearconfig();
            return INITFAIL;
        }

        std::string splitTag;
        splitTag.assign(1, CIni::CFG_DELIMIT_TAG);
        CLstring::splitString(it->second, splitTag, mStageNames);

        for (std::vector<std::string>::iterator it = mStageNames.begin();
                it != mStageNames.end(); it++)
        {
            std::string stageName(*it);

            // Get thread pool
            std::map<std::string, std::string> stageSection =
                    theGlobalProperties()->get(stageName);
            std::map<std::string, std::string>::iterator it;
            std::string threadPoolId = "ThreadId";
            it = stageSection.find(threadPoolId);
            if (it == stageSection.end())
            {
                LOG_ERROR("Not set threadPoolId for %s", stageName.c_str());
                clearconfig();
                return INITFAIL;
            }

            std::string threadName = it->second;
            if (threadName.empty())
            {
                LOG_ERROR("Failed to set %s of the %s",
                        threadPoolId.c_str(), stageName.c_str());
                clearconfig();
                return INITFAIL;
            }
            Threadpool* t = mThreadPools[threadName];


            Stage *stage = StageFactory::makeInstance(stageName);
            if (stage == NULL)
            {
                LOG_ERROR( "Failed to make instance of stage %s",
                        stageName.c_str());
                clearconfig();
                return INITFAIL;
            }
            mStages[stageName] = stage;
            stage->setPool(t);

        } //end for stage

    } catch (std::exception &e)
    {
        LOG_ERROR("Failed to parse stages information, please check, err:%s", e.what());
        clearconfig();
        return INITFAIL;
    }

    if (mStages.size() < 1) {
        LOG_ERROR( "Invalid number of mStages: %u\n", mStages.size());
        clearconfig();
        return INITFAIL;
    }

    return SUCCESS;
}

SedaConfig::status_t
SedaConfig::genNextStages()
{
    try
    {
        for (std::vector<std::string>::iterator it = mStageNames.begin();
                it != mStageNames.end(); it++)
        {

            std::string stageName(*it);
            Stage *stage = mStages[stageName];

            std::map<std::string, std::string> stageSection =
                    theGlobalProperties()->get(stageName);
            std::map<std::string, std::string>::iterator it;
            std::string nextStageId = "NextStages";
            it = stageSection.find(nextStageId);
            if (it == stageSection.end())
            {
                continue;
            }

            std::string nextStageNames = it->second;

            std::vector<std::string> nextStageNameList;
            std::string splitTag;
            splitTag.assign(1, CIni::CFG_DELIMIT_TAG);
            CLstring::splitString(nextStageNames, splitTag, nextStageNameList);

            for (std::vector<std::string>::iterator nextIt =
                    nextStageNameList.begin();
                    nextIt != nextStageNameList.end(); nextIt++)
            {
                std::string &nextStageName = *nextIt;
                Stage *nextStage = mStages[nextStageName];
                stage->pushStage(nextStage);
            }

        } //end for stage
    } catch (std::exception &e)
    {
        LOG_ERROR("Failed to get next stages");
        clearconfig();
        return INITFAIL;
    }
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
    names = mStageNames;
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
