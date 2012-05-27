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

#include <iostream>
#include <sstream>

#include "defs.h"
#include "linit.h"
#include "lang/lstring.h"
#include "trace/log.h"
#include "conf/ini.h"
#include "os/mutex.h"


#include "seda/sedastatsstage.h"
#include "seda/sedastatsevent.h"
#include "seda/sedastats.h"

bool SedaStatsStage::externalCategoryEnableMap[MAX_NUM_CATEGORY] = {false};
bool SedaStatsStage::internalCategoryEnableMap[MAX_NUM_CATEGORY] = {false};

bool SedaStatsStage::collectEnabled = false;
pthread_mutex_t SedaStatsStage::sedaStageLock = MUTEXT_STATIC_INIT();


void SedaStatsStage::manageEvent(StageEvent *event)
{
#if SEDASTATS_MANAGE

    CollectStatsRequest*           collectStatsReq;
    ClearStatsRequest*             clearStatsReq;
    EnableStatsCollectionRequest*  enableStatsReq;
    DisableStatsCollectionRequest* disableStatsReq;

    //Handle the other commEvents sent
    MgmtEvent* mev = dynamic_cast<MgmtEvent*>(event);
    ASSERT(mev, "Expected management event.");

    Request* request = mev->getRequest();
    if (NULL != (collectStatsReq = 
                dynamic_cast<CollectStatsRequest*> (request))){
        LOG_ERROR( "%s", "CollectStatsReq received");
        dumpStats(event);
        return;
    }
    else if (NULL != (clearStatsReq =
                dynamic_cast<ClearStatsRequest*>(request))){
        LOG_ERROR( "%s", "clear stats received");
        clearStats(event);
    }
    else if (NULL != (enableStatsReq =
                dynamic_cast<EnableStatsCollectionRequest*>(request))){
        LOG_ERROR( "%s", "Enable stats received");
        enableCategory(event);
    }
    else if (NULL != (disableStatsReq =
                dynamic_cast<DisableStatsCollectionRequest*>(request))){
        LOG_ERROR( "%s", "Disable stats received");
        disableCategory(event);
    }
    else {
        TRCERR(CtgLog|CtgTrace, "%s", "Unknown event type in handleEvent");
    }
#endif
}

//Handle the Stage events sent
void
SedaStatsStage::handleEvent(StageEvent* event)
{
    SedaStatsEvent* statsEv = NULL;

    if ((statsEv = dynamic_cast<SedaStatsEvent*> (event)))
    {
        storeStats(statsEv);
        return;
    }
    else
    {

        LOG_ERROR("Unknow event type in handleEvent");
        event->done();
    }

    return ;
    
}
//! Parse  properties, instantiate a stage object
/**
 * @pre class members are uninitialized
 * @post initializing the class members
 * @return Stage instantiated object
 */
Stage* 
SedaStatsStage::makeStage(const std::string& tag)
{
    SedaStatsStage* st = new SedaStatsStage(tag.c_str());
    st->setProperties();
    return st;
}

//! Set properties for this object
/**
 * @pre class members are uninitialized
 * @post initializing the class members
 * @return Stage instantiated object
 */
bool 
SedaStatsStage::setProperties()
{
    // 'enabled' flag
    std::string enabled_st;
    enabled_st = theGlobalProperties()->get("SedaStatsEnabled", "false", stageName);
    if (enabled_st.empty() == false)
    {
        CLstring::strToLower(enabled_st);
        if (enabled_st == "true")
        {
            LOG_INFO("Enabling stat recording");

            collectEnabled = true;

            // Enable all categories
            numCatEnabled = MAX_NUM_CATEGORY;
            for(int i = 0; i < MAX_NUM_CATEGORY; ++i)
                externalCategoryEnableMap[i] = true;
        }
    }

    if (collectEnabled)
    {
        LOG_INFO("Enabling stat recording");
    }
    else
    {
        LOG_INFO("Disabling stat recording");
    }

    return true;
}

//! Constructor
/**
 * @param[in] tag     The label that identifies this stage.
 * @pre  tag is non-null and points to null-terminated string
 * @post event queue is empty
 * @post stage is not connected
 */
SedaStatsStage::SedaStatsStage(const char* tag) :
    Stage (tag)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    MUTEX_INIT(&statsStoreLock, &attr);

    numCatEnabled = 0;
    setTheStatsCollectionStage(this);
}

//! Destructor
/**
 * @pre  stage is not connected
 * @post pending events are deleted and stage is destroyed
 */
SedaStatsStage::~SedaStatsStage()
{
    collectEnabled = false;

    //Make sure no one tries to send events
    setTheStatsCollectionStage(NULL);
    
    //clear memory
    CateMapIter cat_it = categoryMap.begin();
    while(categoryMap.end() != cat_it){
        delete cat_it->second;
        ++cat_it;
    }
    categoryMap.clear();

    //Destroy the lock
    MUTEX_DESTROY(&statsStoreLock);
}

//! Initialize stage params and validate outputs
/**
 * @pre  Stage not connected
 * @return TRUE if and only if outputs are valid and init succeeded.
 */
bool 
SedaStatsStage::initialize()
{
    return true;
}

//! Return an error response to the client
/**
 *  Implementation notes:
 *  must call done() on commEv. Also, remember to set commEv ptr to
 *  NULL since this method disposes of the event.
 *
 */
void
SedaStatsStage::errorResponse(StageEvent* ev, 
                              std::string errmsg, 
                              int errcode)
{
#if SEDASTATS_MANAGE
    MgmtEvent* mev = dynamic_cast<MgmtEvent*>(ev);
    ASSERT(mev, "Expected Management Event");
    
    MgmtResponse *resp = new MgmtResponse(errcode,errmsg, true);
    mev->setResponse(resp);
    mev->done();
#endif
}

//Store the stats sent
void
SedaStatsStage::storeStats(SedaStatsEvent* statsEv)
{
    //Get the category and stat
    SedaStats::sedaStatsCategory_t category = statsEv->getCategory();
    
    LOG_DEBUG("category=%d", category);

    if(category <=0 || category >= MAX_NUM_CATEGORY)
    {
        LOG_ERROR("category: %d - dummy category", category);
        statsEv->done();
        return;
    }

    SedaStatsMap* pSedaStatsMap = NULL;

    //Take a lock
    MUTEX_LOCK(&statsStoreLock);

    do {
        //Locate the category sent down
        CateMapIter cat_it = categoryMap.find(category);
        if (cat_it == categoryMap.end()){
            //New category
            pSedaStatsMap = new SedaStatsMap(category);
            if (pSedaStatsMap == NULL)
            {
                LOG_ERROR("Failed to alloc memory for SedaStatsMap");
                break;
            }
            categoryMap[category] = pSedaStatsMap;
        }
        else{
            pSedaStatsMap = cat_it->second;
        }

        pSedaStatsMap->SedaStatsMapStoreStats(statsEv);
    }while(false);

    MUTEX_UNLOCK(&statsStoreLock);

    //Complete the event
    statsEv->done();
    return ;
}

//Enable stats
void
SedaStatsStage::enableCategory(StageEvent* ev)
{   
#if SEDASTATS_MANAGE
    MgmtEvent* mev = dynamic_cast<MgmtEvent*>(ev);
    ASSERT(mev,"Expected Management Event");
    
    Request* request = mev->getRequest();
    EnableStatsCollectionRequest* enableReq =
        dynamic_cast<EnableStatsCollectionRequest*> (request);
    ASSERT(enableReq, "expected enableReq, did not find it");

    //Get category
    int category = enableReq->category();

    if (SedaStats::ALL_CATEGORY == category){
        numCatEnabled = MAX_NUM_CATEGORY;
        //Enable all categories
        for(int i = 0; i < MAX_NUM_CATEGORY; ++i)
            externalCategoryEnableMap[i] = true;
    }
    //if not all categories, check if category is valid
    else if(category > 0 && category < MAX_NUM_CATEGORY)
    {
        //Mark the category to be enabled
        if(false == externalCategoryEnableMap[category])
        {
            externalCategoryEnableMap[category]= true;
            ++numCatEnabled;
        }
    }
    else {
        //send error
        errorResponse(ev, "Invalid category", STATS_INVALID_INPUT);
        return;
    }

    collectEnabled = true;

    int                             errcode     = MAUI_SUCCESS;
    std::string                     errmsg      = "success";
    bool                            exception   = false;

    EnableStatsCollectionResponse* enableStatsResp =
        new EnableStatsCollectionResponse(errcode,
                                                errmsg,
                                                exception);
    ASSERT(enableStatsResp, "Could not create the enable stats response");
    mev->setResponse(enableStatsResp);
    mev->done();
#endif
    return;
}


//Disable a category
void
SedaStatsStage::disableCategory(StageEvent* ev)
{
#if SEDASTATS_MANAGE
    MgmtEvent* mev = dynamic_cast<MgmtEvent*>(ev);
    ASSERT(mev, "Expected Management Event");
    
    Request* request = mev->getRequest();
    DisableStatsCollectionRequest* disableReq =
        dynamic_cast<DisableStatsCollectionRequest*> (request);
    ASSERT(disableReq, "expected disableReq, did not find it");

    //get the category
    int category = disableReq->category();
    
    if (SedaStats::ALL_CATEGORY == category){
        numCatEnabled = 0;
        //Mark all categories as disabled
        for(int i = 0; i < MAX_NUM_CATEGORY; ++i)
            externalCategoryEnableMap[i] = false;
        //Remove stats of all categories
        _removeCategory(SedaStats::ALL_CATEGORY);

        collectEnabled = false;
    }
    //validate if the category is legal
    else if(category > SedaStats::ALL_CATEGORY && 
            category < MAX_NUM_CATEGORY)
    {
        if(true == externalCategoryEnableMap[category])
        {
            //Mark the category as disabled
            externalCategoryEnableMap[category] = false;
            if(0 == --numCatEnabled)
                //If there are no more categories enabled
                //then mark all categories as disabled
                externalCategoryEnableMap[SedaStats::ALL_CATEGORY] = false;
            //Remove stats for this category
            _removeCategory((SedaStats::sedaStatsCategory_t)category);
        }
    }
    else {
        //send error
        errorResponse(ev, "Invalid category or stat", 
                      STATS_INVALID_INPUT);
        return;
    }

    int                             errcode     = MAUI_SUCCESS;
    std::string                     errmsg      = "success";
    bool                            exception   = false;
    std::auto_ptr<Response>   response;

    DisableStatsCollectionResponse* disableStatsResp =
        new DisableStatsCollectionResponse(errcode,
                                                 errmsg,
                                                 exception);
    ASSERT(disableStatsResp, "Could not create the DisableStatsResponse");
    mev->setResponse(disableStatsResp);
    mev->done();
#endif
    return;
}

//Return the stats collected
void
SedaStatsStage::dumpStats(StageEvent* ev)
{
#if SEDASTATS_MANAGE
    /*While returning stats the following are possible
     * 1) Return all stats under a category
     * 2) Return a specific stat under a specific category
     * 3) Return all categories and all stats
     * 4) Return a specific stat from any category
     *
     * Though 4) is possible, it would imply 
     * that the same statId is being paired with multiple 
     * categories.
     */
    MgmtEvent* mev = dynamic_cast<MgmtEvent*>(ev);
    ASSERT(mev, "Expected Management Event");

    Request* request = mev->getRequest();
    CollectStatsRequest* statsReq =
        dynamic_cast<CollectStatsRequest*> (request);
    ASSERT(statsReq, "expected statsReq, did not find it");
    
    SedaStats::sedaStatsCategory_t   category = SedaStats::ALL_CATEGORY;
    SedaStats::sedaStatsIdentifier_t statsId  = SedaStats::ALL_STATS;
    std::string                      statsIdStr;
    bool                             statsId_present    = false;
    bool                             statsIdStr_present = false;
    
    // Check cegegory
    if(statsReq->category().present())
    {
        unsigned int iCategory = statsReq->category().get();
        if(iCategory >= SedaStats::DUMMY_END_CATEGORY)
        {
            //send error response
            errorResponse(ev, "Invalid category", STATS_INVALID_INPUT);
            return;
        }
        category = (SedaStats::sedaStatsCategory_t) iCategory;
    }

    // Check stat id
    if(statsReq->statsId().present())
    {
        int iStatsId = statsReq->statsId().get();
        if(iStatsId < 0 || iStatsId >= SedaStats::DUMMY_END_STAT)
        {
            errorResponse(ev, "Invalid stats", STATS_INVALID_INPUT);
            return;
        }
        statsId = (SedaStats::sedaStatsIdentifier_t)iStatsId;
        statsId_present = true;
    }
    // Check stat id string
    else if(statsReq->statsIdStr().present())
    {
        statsIdStr = statsReq->statsIdStr().get();
        statsIdStr_present = true;
    }

    bool success;
    std::vector<basicStats> vStats;
    
    //Dump stats for category/stats, no matter category is enabled or not
    if(statsId_present)
        success = _dumpStats(category, SedaStats::StatsId(statsId), vStats);
    else if(statsIdStr_present)
        success = _dumpStats(category, SedaStats::StatsId(statsIdStr), vStats);
    else
        success = _dumpStats(category, SedaStats::StatsId(), vStats);
    
    if(!success)
    {
        errorResponse(ev, "Category/stats not found", STATS_CATEGORY_NOT_FOUND);
        return;
    }

    int          errcode       = MAUI_SUCCESS;
    std::string  errmsg        = "success";
    bool         exception     = false;
    int          categoryCount = categoryMap.size(); 

    //Create a response which indicates if the category is enabled
    CollectStatsResponse* statsResp =
        new CollectStatsResponse(errcode,
                                       errmsg,
                                       exception,
                                       categoryCount,
                                       externalCategoryEnableMap[category]);
    ASSERT(statsResp, "Could not create stats resp");

    // check to see which component version is currently running
    AtmosUpgradeMgr* upgradeMgr = AtmosUpgradeMgr::getInstance();
    bool includeIncrementStats = false;
    if ((upgradeMgr) &&
        (upgradeMgr->checkUpgradeItem(AtmosUpgradeMgr::SEDA_STATS_COUNTER_INFO)))
        includeIncrementStats = true;
    
    for(std::vector<basicStats>::iterator iter = vStats.begin();
        iter != vStats.end(); ++iter) {
        // filter out new counter stat fields as needed
        if (false == includeIncrementStats) {
            if (iter->avgIncrementVal().present())
                iter->avgIncrementVal().reset();
            if (iter->minIncrementVal().present())
                iter->minIncrementVal().reset();
            if (iter->maxIncrementVal().present())
                iter->maxIncrementVal().reset();
        }
        statsResp->basicStats().push_back(*iter);
    }
    mev->setResponse(statsResp);
    mev->done();
#endif
    return;
}

//Clear stats
void
SedaStatsStage::clearStats(StageEvent* ev)
{
#if SEDASTATS_MANAGE
    MgmtEvent* mev = dynamic_cast<MgmtEvent*>(ev);
    ASSERT(mev, "Expected Management Event");
    
    Request* request = mev->getRequest();
    ClearStatsRequest* statsReq =
        dynamic_cast<ClearStatsRequest*> (request);
    ASSERT(statsReq, "expected clearStatsReq, did not find it");

    SedaStats::sedaStatsCategory_t   category = SedaStats::ALL_CATEGORY;
    SedaStats::sedaStatsIdentifier_t statsId;
    std::string                      statsIdStr;
    bool                             statsId_present    = false;
    bool                             statsIdStr_present = false;

    // Check cegegory
    if(statsReq->category().present())
    {
        unsigned int iCategory = statsReq->category().get();
        if(iCategory >= SedaStats::DUMMY_END_CATEGORY)
        {
            //send error response
            errorResponse(ev, "Invalid category", STATS_INVALID_INPUT);
            return;
        }
        category = (SedaStats::sedaStatsCategory_t) iCategory;
    }

    // Check stat id
    if(statsReq->statsId().present())
    {
        int iStatsId = statsReq->statsId().get();
        if(iStatsId < 0 || iStatsId >= SedaStats::DUMMY_END_STAT)
        {
            errorResponse(ev, "Invalid stats", STATS_INVALID_INPUT);
            return;
        }
        statsId = (SedaStats::sedaStatsIdentifier_t)iStatsId;
        statsId_present = true;
    }
    // Check stat id string
    else if(statsReq->statsIdStr().present())
    {
        statsIdStr = statsReq->statsIdStr().get();
        statsIdStr_present = true;
    }

    bool success;

    if(statsId_present)
        success = _clearStats(category, SedaStats::StatsId(statsId));
    else if(statsIdStr_present)
        success = _clearStats(category, SedaStats::StatsId(statsIdStr));
    else 
        success = _clearStats(category, SedaStats::StatsId());
    
    if(!success)
    {
        errorResponse(ev, "category/stat not found", STATS_STAT_NOT_FOUND);
        return;
    }

    int                             errcode     = SUCCESS;
    std::string                     errmsg      = "success";
    bool                            exception   = false;

    ClearStatsResponse* clearStatsResp =
        new ClearStatsResponse(errcode, errmsg, exception);
    ASSERT(clearStatsResp, "Could not create the ClearStatsResponse");

    mev->setResponse(clearStatsResp);
    mev->done();
#endif
    return;
}

void 
SedaStatsStage::enableCategory(SedaStats::sedaStatsCategory_t category)
{
    if(category == SedaStats::ALL_CATEGORY)
    {
        for(int i = 0; i < MAX_NUM_CATEGORY; ++i)
            internalCategoryEnableMap[i] = true;
    }
    else if(category > 0 && category < MAX_NUM_CATEGORY)
        internalCategoryEnableMap[category] = true;
}

void
SedaStatsStage::disableCategory(SedaStats::sedaStatsCategory_t category)
{
    if(category == SedaStats::ALL_CATEGORY)
    {
        for(int i = 0; i < MAX_NUM_CATEGORY; ++i)
            internalCategoryEnableMap[i] = false;
        if(NULL !=  theStatsCollectionStage())
            theStatsCollectionStage()->_removeCategory(category);
    }
    else if(category > SedaStats::ALL_CATEGORY && 
            category < SedaStats::DUMMY_END_CATEGORY)
    {
        internalCategoryEnableMap[category] = false;
        if(NULL !=  theStatsCollectionStage())
            theStatsCollectionStage()->_removeCategory(category);
    }
}

bool
SedaStatsStage::dumpStats(SedaStats::sedaStatsCategory_t   category,
                          const SedaStats::StatsId&        sid,
                          std::string&                     output)
{
    return (NULL != theStatsCollectionStage()) && 
            theStatsCollectionStage()->_dumpStats(category, sid, output);
}

bool
SedaStatsStage::_dumpStats(SedaStats::sedaStatsCategory_t   category,
                           const SedaStats::StatsId&        sid,
                           std::string&                     output) const
{
    bool found = true;
    MUTEX_LOCK(&statsStoreLock);
    if(SedaStats::ALL_CATEGORY != category)
    {
        CateMapConstIter cat_it = categoryMap.find(category);
        if (categoryMap.end() != cat_it)
            found = cat_it->second->SedaStatsMapDumpStats(sid, output);
        else
            found = false;
    }
    else
    { 
        CateMapConstIter cat_it = categoryMap.begin();
        while(categoryMap.end() != cat_it)
        {
            cat_it->second->SedaStatsMapDumpStats(sid, output);
            ++cat_it;
        }
    }
    MUTEX_UNLOCK(&statsStoreLock);
    return found;
}


bool
SedaStatsStage::clearStats(SedaStats::sedaStatsCategory_t  category,
                           const SedaStats::StatsId&       sid)
{
    return (NULL != theStatsCollectionStage()) && 
            theStatsCollectionStage()->_clearStats(category, sid);
}

bool
SedaStatsStage::_clearStats(SedaStats::sedaStatsCategory_t  category,
                            const SedaStats::StatsId&       sid)
{
    LOG_DEBUG("category=%d, sid=%d, sidstr=%s",
            category, sid.id, sid.strId.c_str());
    bool found = true;
    MUTEX_LOCK(&statsStoreLock);
    if(SedaStats::ALL_CATEGORY != category)
    {
        CateMapIter cat_it = categoryMap.find(category);
        found = (categoryMap.end() != cat_it);
        if (found)
        {
            bool rmFlag = !isCategoryEnabled(category);
            if(sid.type == SedaStats::StatsId::ALL && rmFlag)
            {
                delete cat_it->second;
                categoryMap.erase(cat_it);
            }
            else
                found = cat_it->second->SedaStatsMapClearStats(sid, rmFlag);
        }
    }
    else
    {
        CateMapIter cat_it = categoryMap.begin();
        while(categoryMap.end() != cat_it)
        {
            bool rmFlag = !isCategoryEnabled(cat_it->first);
            if(sid.type == SedaStats::StatsId::ALL && rmFlag)
            {
                delete cat_it->second;
                categoryMap.erase(cat_it++);
            }
            else
            {
                cat_it->second->SedaStatsMapClearStats(sid, rmFlag);
                ++cat_it;
            }
        }
    }
    MUTEX_UNLOCK(&statsStoreLock);
    return found;
}

bool 
SedaStatsStage::_removeCategory(SedaStats::sedaStatsCategory_t category)
{
    LOG_DEBUG("category=%d", category);
    bool found = true;
    MUTEX_LOCK(&statsStoreLock);
    if(SedaStats::ALL_CATEGORY != category)
    {
        bool should_remove = !isCategoryEnabled(category);
        if(should_remove)
        {
            CateMapIter cat_it = categoryMap.find(category);
            if (categoryMap.end() != cat_it)
            {
                delete cat_it->second;
                categoryMap.erase(cat_it);
            }
            else
                found = false;
        }
    }
    else
    {
        CateMapIter cat_it = categoryMap.begin();
        while(categoryMap.end() != cat_it)
        {
            bool should_remove = !isCategoryEnabled(cat_it->first);
            if(should_remove)
            {
                delete cat_it->second;
                categoryMap.erase(cat_it++);
            }
            else
                ++cat_it;
        }
    }
    MUTEX_UNLOCK(&statsStoreLock);
    return found;
}

//Add the stats to the stage 
void 
SedaStatsStage::addStatsEvent(StageEvent *event)
{

    MUTEX_LOCK(&sedaStageLock);

    theStatsCollectionStage()->addEvent(event);

    MUTEX_UNLOCK(&sedaStageLock);
}

//Accessory method to the stats stage pointer
SedaStatsStage*&
SedaStatsStage::theStatsCollectionStage(){
    static SedaStatsStage* stg = NULL;
    return stg;
}

//Set the pointer to the pointer to the stats stage
void
SedaStatsStage::setTheStatsCollectionStage( SedaStatsStage *stg){
    MUTEX_LOCK(&sedaStageLock);
    theStatsCollectionStage() = stg;
    MUTEX_UNLOCK(&sedaStageLock);
}

//! Check if the category is enabled for stats collection
/**
 *  @brief Stats can be enabled per category. Call this method and pass
 *          the category to see if it is enabled
 *  @param  category    The category to be checked
 *  @return bool        true if the category is enabled else false
 */
bool
SedaStatsStage::isCategoryEnabled(SedaStats::sedaStatsCategory_t category)
{
    bool ret = false;
    //Check if the stats stage has been initialized.
    if (true == collectEnabled){
        //Category can only be between 0 and MAX_NUM_CATEGORY
        if (category < MAX_NUM_CATEGORY && category >= 0){
            ret = (externalCategoryEnableMap[category] || 
                    internalCategoryEnableMap[category]);
        }
    }
    return ret;
}

//Implementation of SedaStatsMap

//Constructor
SedaStatsMap::SedaStatsMap(SedaStats::sedaStatsCategory_t category):
    _category(category)
{}

//Destructor
SedaStatsMap::~SedaStatsMap()
{
    IDStoreMap::const_iterator it = statsIdMap.begin();
    while( it != statsIdMap.end())
    {
        delete it->second;
        ++it;
    }
    statsIdMap.clear();

    StrStoreMap::const_iterator it2 = statsStrIdMap.begin();
    while( it2 != statsStrIdMap.end())
    {
        delete it2->second;
        ++it2;
    }
    statsStrIdMap.clear();
}


//! Store the stat in the map
/**
 *  @brief store the stat being reported. 
 *  @param  statsEv stats event sent, event is completed here
 */
void SedaStatsMap::SedaStatsMapStoreStats(SedaStatsEvent* statsEv){
    
    /* 1) Find the stat id being reported
     * 2) If present add the values reported
     * 3) else create a new entry in the map and store the stats
     */

    //Get the stat id being reported
    const SedaStats::StatsId& statsId = statsEv->getStatID();

    SedaStatsStore* st = NULL;
    if(statsId.type == SedaStats::StatsId::ID)
    {
        IDStoreMap::const_iterator ipos = statsIdMap.find(statsId.id);
        if(ipos != statsIdMap.end())
        {
            st = (*ipos).second;
        }
        else
        {
            st = new SedaStatsStore(statsEv->isPersistent());
            if (st == NULL)
            {
                LOG_ERROR("Failed to alloc memory for SedaStatsStore");
                return ;
            }
            statsIdMap[statsId.id] = st;
        }
    }
    else
    {
        StrStoreMap::const_iterator ipos = statsStrIdMap.find(statsId.strId);
        if(ipos != statsStrIdMap.end())
        {
            st = (*ipos).second;
        }
        else
        {
            st = new SedaStatsStore(statsEv->isPersistent());
            if (st == NULL)
            {
                LOG_ERROR("Failed to alloc memory for SedaStatsStore");
                return ;
            }
            statsStrIdMap[statsId.strId] = st;
        }
    }
    
    //Time duration reported - add the time to the total time
    if(statsEv->hasTime())
        st->incTotalTime(statsEv->getTime());

    //If also used as a counter, record the counter value
    if (true == statsEv->resetCount()){
        st->resetCounter(statsEv->getStatCount());
    }
    else if (true == statsEv->hasCount()){
        st->incCounter(statsEv->getStatCount());
    }
}

//! Return the stat being collected
/**
 *  @brief This method is called to retrieve the stats stored
 *  @param  statsId     This is the stat id which need to be collected
 *          statsResp   The Response object in which the stats are copied
 *  @return bool        true if stat is found else false
 */

bool 
SedaStatsMap::SedaStatsMapDumpStats(const SedaStats::StatsId& sid,
        std::string &output) const
{
    if(sid.type == SedaStats::StatsId::ALL)
    {
        output += SedaStats::categoryStr(_category) + "\n";
        // dump statsIdMap
        IDStoreMap::const_iterator it = statsIdMap.begin();
        while (it != statsIdMap.end()){
            output +=  "\t" + SedaStats::getIdStr(it->first) + "\n";

            dumpStore(it->second, output);
            ++it;
        }

        output += "\n";
        // dump statsStrIdMap
        StrStoreMap::const_iterator it2 = statsStrIdMap.begin();
        while (it2 != statsStrIdMap.end()){
            output += "\t" + (it2->first) + "\n";

            dumpStore(it2->second, output);
            ++it2;
        }
        return true;
    }

    SedaStatsStore* store = NULL;
    
    unsigned int idInt;
    std::string idStr;
    
    //Find the stat
    if(sid.type == SedaStats::StatsId::ID)
    {
        IDStoreMap::const_iterator it = statsIdMap.find(sid.id);
        if (statsIdMap.end() != it )
        {
            store = it->second;
            idInt = sid.id;
            idStr = SedaStats::getIdStr(sid.id);
        }
    }
    else if(sid.type == SedaStats::StatsId::STR)
    {
        StrStoreMap::const_iterator it = statsStrIdMap.find(sid.strId);
        if (statsStrIdMap.end() != it )
        {
            store = it->second;
            idInt = 0;
            idStr = sid.strId;
        }
    }
    if(NULL != store)
    {
        output += SedaStats::categoryStr(_category) + "\n";
        output +=  "\t" + idStr + "\n";
        dumpStore(store, output);
    }
    return (NULL != store);
}

void SedaStatsMap::dumpStore(SedaStatsStore* store, std::string &output) const
{
    std::ostringstream oss;

    oss << "\t\t" << "AverageTime:" << store->getAvgTime() << "\n"
        << "\t\t" << "MaxTime:"     << store->getMaxTime() << "\n"
        << "\t\t" << "MinTIme:"     << store->getMinTime() << "\n"
        << "\t\t" << "NumStats:"    << store->getNumStats() << "\n";

    //If store is used as a counter
    if (store->isCounter()){
        //total counter
        oss << "\t\t" << "TotalCounter:"   << store->getTotalCounter() << "\n"
            << "\t\t" << "AverageCounter:" << store->getAvgIncrement() << "\n"
            << "\t\t" << "MaxCounter:"     << store->getMaxIncrement() << "\n"
            << "\t\t" << "MinCounter:"     << store->getMinIncrement() << "\n"
            << "\t\t" << "CounterCount:"   << store->getCounterCount() << "\n";

    }

    output += oss.str();
}


//! Clear the stat from the Stats Map
/**
 *  @brief Clear the stat sent down 
 *  @param  statsId id of the stat to be cleared
 *  @return true if the stat is found else false
 */
bool 
SedaStatsMap::SedaStatsMapClearStats(const SedaStats::StatsId& sid, 
                                     bool  removeflag)
{
    if(sid.type == SedaStats::StatsId::ALL)
    {
        // Clear statsIdMap
        IDStoreMap::iterator it = statsIdMap.begin();
        while(it != statsIdMap.end())
        {
            if(it->second->isPersistent())
            {
                ++it;
                continue;
            }
            
            //Clear only if persistent flag is not set
            if(!removeflag)
            {
                it->second->reset();
                ++it;
            }
            else
            {
                //delete the entry
                delete it->second;
                //clear the map
                statsIdMap.erase(it++);
            }
        }

        // Clear statsStrIdMap
        StrStoreMap::iterator it2 = statsStrIdMap.begin();
        while(it2 != statsStrIdMap.end())
        {
            if (it2->second->isPersistent())
            {
                ++it2;
                continue;
            }
            if(!removeflag)
            {
                it2->second->reset();
                ++it2;
            }
            else
            {
                //delete the entry
                delete it2->second;
                //clear the map
                statsStrIdMap.erase(it2++);
            }
        }

        return true;
    }

    bool found = false;
    if(sid.type == SedaStats::StatsId::ID)
    {
        IDStoreMap::iterator it = statsIdMap.find(sid.id);
        if(statsIdMap.end() != it)
        {
            //Clear only if persistent flag is not set
            if(!(it->second->isPersistent()))
            {
                if(!removeflag)
                    it->second->reset();
                else
                {
                    delete it->second;
                    statsIdMap.erase(it);
                }
            }
            found = true;
        }
    }
    else
    {
        StrStoreMap::iterator it = statsStrIdMap.find(sid.strId);
        if(statsStrIdMap.end() != it)
        {
            //Clear only if persistent flag is not set
            if(!(it->second->isPersistent()))
            {
                if(!removeflag)
                    it->second->reset();
                else
                {
                    delete it->second;
                    statsStrIdMap.erase(it);
                }
            }
            found = true;
        }
    }
    
    return found;
}

