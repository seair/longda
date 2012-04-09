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


#ifndef __SEDA_CONFIG__
#define __SEDA_CONFIG__


// Include Files
#include <memory>   // std::auto_ptr
#include <vector>
#include <map>
#include <string>

#include "seda/threadpool.h"


/** 
 *  @file
 *  @author Longda Feng
 *  @date   4/6/07
 */

//! A class to configure seda stages
/** 
 *  Each application uses an xml file to define the stages that make up the
 *  application, the threadpool that the stages use, and the parameters that
 *  are passed to configure each individual stage.  The SedaConfig class
 *  consumes this xml file, parses it, and instantiates the indicated
 *  configuration.  It also then provides access to individual mStages and
 *  threadpools within the configuration. The parameters passed to each
 *  stage consists of the global attributes defined for all the seda mStages
 *  in the seda instance as well as the attributes defined for that specific
 *  stage. The attributes defined for each stage will override the global
 *  attributes in case of duplicate attributes
 */

class SedaConfig {

public:

    typedef enum {
        SUCCESS = 0,
        INITFAIL,
        PARSEFAIL
    } status_t;

    static SedaConfig*& getInstance();

    //! Destructor
    /**
     * @post configuration is deleted
     */
    ~SedaConfig();

    //! Set the file holding the configuration
    /**
     * @pre  filename is a null-terminated string, or \c NULL
     * @post config filename is initialized, config string is empty
     */
    void setCfgFilename(const char* filename);

    //! Set the string holding the configuration
    /**
     * @pre  configStr is a null-terminated string, or \c NULL
     * @post config string is initialized, config filename is empty
     */
    void setCfgString(const char* configStr);

    //! Parse config file or string
    /**
     * parse the seda config file or string and build an in-memory
     * representation of the config.  Also, update global properties object
     * with global seda properties from the config.
     *
     * @post config file or string is parsed and global seda properties
     *       are added to global properties object
     * @returns SUCCESS if parsing succeeds
     *          PARSEFAIL if parsing fails
     */
    status_t parse();

    //! instantiate the parsed configuration
    /**
     * Use the parsed configuration to instantiate the thread pools 
     * and mStages specificed, but do not start it running.
     *
     * @pre  configuration has been successfully parsed
     * @post upon SUCCESS, thread pools and mStages are created,
     *        ready to be started.
     */
    status_t instantiateCfg();

    //! start the parsed, instantiated configuration
    /**
     * @pre   configuration parsed and instantiated
     * @post  if SUCCESS, the SEDA pipleine is now running.
     *
     */
    status_t start();

    //! Complete Initialization of the mThreadPools and mStages
    /**
     * Use the parsed config to initialize the required mThreadPools and
     * mStages, and start them running.  If the config has not yet been
     * parsed then try to parse it first.  The init function combines 
     * parse(), instantiate() and start()
     *
     * @pre empty mThreadPools and mStages
     * @post if returns SUCCESS then 
     *          mThreadPools and mStages created/initialized and running
     * @post if returns INITFAIL or PARSEFAIL then
     *          mThreadPools and stage list are empty
     */
    status_t init();

    //! Clean-up the threadpool and mStages
    /**
     * @post all mStages disconnected and deleted, all mThreadPools deleted
     */
    void cleanup();

    //! get the desired stage given a string
    /**
     *
     * @param[in] stagename   take in the stage name and convert it to a Stage
     * @pre 
     * @return a reference to the Stage
     */    
    Stage* getStage(const char* stagename);

    //! get the desired threadpool a string
    /**
     *
     * @param[in] index   take in the index for threadpool
     * @pre 
     * @return a reference to the ThreadPool
     */
    Threadpool& getThreadPool(const int index);
    

    //! Get a list of all stage names
    /**
     * @param[in/out] names   names of all mStages
     */
    void getStageNames(std::vector<std::string>& names) const;

    //! Query the number of queued events at each stage.
    /**
     * @param[in/out] stats   number of events enqueued at each
     *   stage.  
     */
    void getStageQueueStatus(std::vector<int>& stats) const;

    std::map<std::string, Stage *>::iterator begin();
    std::map<std::string, Stage *>::iterator end();

private:
    
    const static std::string SEDA_BASE_NAME;
    const static std::string THREAD_POOLS_NAME;

    static SedaConfig* instance;

    // In old logic, SedaConfig will parse seda configure file
    // but here, only one configure file
    std::string mCfgFile;
    std::string mCfgStr;
    
    std::map<std::string, Threadpool *> mThreadPools;
    std::map<std::string, Stage *>      mStages;
    std::vector<std::string>            mStageNames;
    
    //! Constructor
    SedaConfig();

    //! instantiate the mThreadPools and mStages
    /**
     * Instantiate the mThreadPools and mStages defined in the configuration
     *
     * @pre  cfg_ptr is not NULL
     * @post returns SUCCESS ==> all mThreadPools and mStages are created
     *       returns INITFAIL ==> mThreadPools and mStages are deleted
     */
    status_t instantiate();

    status_t initThreadPool();
    status_t initStages();
    status_t genNextStages();

    //! delete all mThreadPools and mStages
    /**
     * @pre  all existing mStages are disconnected
     * @post all mThreadPools and mStages are deleted
     */
    void clearconfig();

    //! init event history setting
    /**
     * Setting theMaxEventHops, theEventHistoryFlag
     */
    void initEventHistory();

    status_t initThreadpool();

    SedaConfig& operator=(const SedaConfig& cevtout);

};

inline std::map<std::string, Stage *>::iterator SedaConfig::begin()
{
    return mStages.begin();
}

inline std::map<std::string, Stage *>::iterator SedaConfig::end()
{
    return mStages.end();
}

inline Stage* SedaConfig::getStage(const char* stagename) {
    if (stagename) {
        std::string sname(stagename);
        return mStages[stagename];
    }
    return NULL;
}

//! Global seda config object
SedaConfig*& theSedaConfig();

bool& theEventHistoryFlag();
u32_t& theMaxEventHops();

#endif //__SEDA_CONFIG__
