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



/// 
/// @file sedastatsstage.h
/// @brief Implementation of the stats collection stage
//          within seda framework
/// @author Longda
/// @date 2007-10-22
/// 

#ifndef _SEDA_STATS_STAGE_H
#define _SEDA_STATS_STAGE_H

#include <vector>
#include <map>

#include "seda/stage.h"
#include "seda/sedastatsevent.h"
#include "seda/sedastats.h"


class SedaStatsStore;
class SedaStatsMap;

#define SEDASTATS_MANAGE   0

/**
 *  \class SedaStatsStage
 *  \brief  Contains the implementation of the seda statistics collection 
 *          stage. 
 */
class SedaStatsStage : public Stage
{
public:
    //! Constructor for SedaStatsStage
    /**
     *  @brief Constructor for seda stats collection stage
     *  @param[in] tag  The label that identifies this stage
     *  @pre  tag is non-null and points to null-terminated string
     *  @post event queue is empty
     *  @post stage is not connected
     */
    SedaStatsStage(const char* tag);

    //! Destructor
    /**
     *  @brief 
     *  @pre  stage is not connected
     *  @post pending events are deleted and stage is destroyed
     */
    ~SedaStatsStage();

    //! Parse  properties, instantiate a summation stage object
    /**
     * @pre class members are uninitialized
     * @post initializing the class members
     * @return Stage instantiated object
     */
    static Stage* makeStage(const std::string& tag);


    //! Add the stats event to the stats collection stage
    /**
     *  @brief This will safely add the event to the queue of
     *          the stats collection stage
     *  @param[in] Ev   event to be added
     */
    static void 
        addStatsEvent(StageEvent* Ev);

    //! Check if stats collection is enabled
    static bool isCollectEnabled() { return collectEnabled; }

    //! Check if stats collection is enabled for the category
    /**
     *  @brief returns a bool indicating is stats collection has been enabled
     *  @param  category    category to query
     *  @return bool yes is stats collection is enabled
     */
    static bool
        isCategoryEnabled(SedaStats::sedaStatsCategory_t category);

    //! Internally enable/disable stats collection for a category
    static void
        enableCategory(SedaStats::sedaStatsCategory_t category);

    static void
        disableCategory(SedaStats::sedaStatsCategory_t category);

    //! Dump stats
#if SEDASTATS_MANAGE
    static bool
        dumpStats(SedaStats::sedaStatsCategory_t category, 
                  const SedaStats::StatsId&      sid,
                  std::vector<maui::basicStats>& vStats);
#endif

    //! Clear stats
    static bool 
        clearStats(SedaStats::sedaStatsCategory_t category, 
                   const SedaStats::StatsId&      sid);
protected:

    //! Initialize stage params and validate outputs
    /**
     * @pre  Stage not connected
     * @return TRUE if and only if outputs are valid and init succeeded.
     */
    bool initialize();

    //! Set properties for this object
    /**
     * @pre class members are uninitialized
     * @post initializing the class members
     * @return Stage instantiated object
     */
    bool setProperties();

 
    //! Handle the event sent to this stage
    /**
     *  @brief Handle events sent down to this stage
     *  @param  event   The stage event sent down
     */
    void  handleEvent(StageEvent* event);
    

    //! Handle callbacks
    /**
     *  @brief This stage does not expect any callbacks
     *  @param  
     *  @return 
     */
    void callbackEvent(StageEvent* event, CallbackContext* context){
        ASSERT(false,"%s", "No callback expected\n");
    }

private:
    //! Return an error response to the client
    /**
     *  Implementation notes:
     *  must call done() on commEv. Also, remember to set commEv ptr to
     *  NULL since this method disposes of the event.
     */
    void errorResponse(StageEvent* ev, std::string errmsg, int errcode);

   //! Store the stats sent in the event
    /**
     *  @brief      The stats sent are stored for retrieval later
     *  @param[in]  The stats event sent
     *  @return     
     */
    void
        storeStats(SedaStatsEvent* statsEv);

    //! Handle enable stats command to the stage
    /**
     *  @brief The stats collection enable command is handled here
     *  @param[in/out]  commEv the communication event sent down  
     */
    void
        enableCategory(StageEvent* ev);

    //! Handle disable stats command to the stage
    /**
     *  @brief The stats collection disable command is handled here
     *  @param[in/out]  commEv the communication event sent down  
     */
    void
        disableCategory(StageEvent* ev);
   
    //! Handle the command to return the stats collected
    /**
     *  @brief All stats collected so far are returned and they are cleared
     *          from in memory
     *  @param[in] commEv   the communication object used to send the 
     *                      response
     *  @return 
     */
    void
        dumpStats(StageEvent* ev);

    //! Clear a stat or category collected
    /**
     *  @brief The stats collected need to be explicitly cleares
     *          the client can either send the category or the statid
     *          to be cleared. If neither category nor stat id is sent
     *          then all stats are cleared.
     *  @param  CommEv communication event sent
     */
    void clearStats(StageEvent* ev);

    bool 
        _removeCategory(SedaStats::sedaStatsCategory_t category);
#if SEDASTATS_MANAGE
    bool 
        _dumpStats(SedaStats::sedaStatsCategory_t category,
                   const SedaStats::StatsId&      sid,
                   std::vector<maui::basicStats>& vStats) const;
#endif
    bool
        _clearStats(SedaStats::sedaStatsCategory_t category,
                    const SedaStats::StatsId&      sid);

    //! Global accessor method to the Stats Collection Stage
    /**
     *  @brief This method is kept private as this should be used
     *          only internally
     *  @return the stats collection stage
     */
    static SedaStatsStage*& theStatsCollectionStage();

    //! Set the stats collection stage to
    /**
     *  @brief  Safely Set or unset the pointer to the stage
     *  @param[in]  stg pointer to the stage
     *  @return
     */
    static void setTheStatsCollectionStage( SedaStatsStage* stg);

    static bool collectEnabled;

    static  pthread_mutex_t sedaStageLock;
    mutable pthread_mutex_t statsStoreLock;//!< Lock for storing stats

    /**
     *  \brief Array of enable flags for various categories
     *          To maintain quick lookup of enable flags and
     *          avoid having locks around maps for storing this
     *          information, a static array is preallocated
     *          which is initialized to false in the constructor
     *          for the stage
     */
    static bool externalCategoryEnableMap[MAX_NUM_CATEGORY];
    static bool internalCategoryEnableMap[MAX_NUM_CATEGORY];

    /**
     *  \brief Map of stats collected
     */
    typedef std::map<SedaStats::sedaStatsCategory_t, SedaStatsMap*> CateMap;
    typedef CateMap::iterator               CateMapIter;
    typedef CateMap::const_iterator         CateMapConstIter;

    CateMap categoryMap;

    /** Count of categories recorded */
    int categoryCount;

    /** Count of num of categories enabled */
    int numCatEnabled;
};

/**
 *  \class Wrapper class for map containing stat ids and actual stats
 *  \brief  This is to simplify storing stats under a category map. 
 */
class SedaStatsMap{
public:
    
    //! Constructor
    /**
     *  @brief 1) Save Category ID under which it is created
     *  @param[in] category the category which this map belongs to
     */
    SedaStatsMap(SedaStats::sedaStatsCategory_t category);


    //! Destructor
    /**
     *  @brief 1) Clear the map of all enteries
     */
    ~SedaStatsMap();


    //! Store the information in the event sent
    /**
     *  @brief The actual copying of the stats is done here
     *  @param [in] statsEv the event which contains all the information
     *  @return 
     */
    void SedaStatsMapStoreStats(SedaStatsEvent* statsEv);
    
    //!  Find the stat requested and complete the event
    /**
     *  @brief  The stat requested is searched in the map
     *          if it is found then the response is filled
     *          and sent back else a false is returned
     *  @param commEv   the communication event sent
     *                  to the stage
     *  @return true if the stat is found, else return false
     */
#if SEDASTATS_MANAGE
    bool SedaStatsMapDumpStats(const SedaStats::StatsId& sid,
                               std::vector<maui::basicStats>& vStats) const;
#endif
    //! Find the specific stat and clear the record
    /**
     *  @brief If a unique stat id is sent then that stat is cleared
     *          If enum for all stat is sent, then all stats in the 
     *          map are cleared
     *  @param  statsId stat id which indicated which stat to clear
     *  @param  removeflag  If set to false, reset corresponding 
                            SedaStatsStore; else, delete it.
     *  @return true if the stat was present, false if the stat was not
     *              present
     */
    bool SedaStatsMapClearStats(const SedaStats::StatsId& sid,
                             bool  removeflag);

private:
#if SEDASTATS_MANAGE
    //! Dump StatsStore into a maui::basicStats structure
    void dumpStore(SedaStatsStore* store, maui::basicStats& basicSts)
        const;
#endif    
    SedaStats::sedaStatsCategory_t  _category;  /**< \brief category for this map */

    typedef std::map<SedaStats::sedaStatsIdentifier_t, SedaStatsStore*> 
                                                       IDStoreMap;
    typedef std::map<std::string, SedaStatsStore*>          StrStoreMap;
    
    /** The map for storing the stats id and stats */
    IDStoreMap  statsIdMap;
    /** The map for storing the stats id(string) and stats */
    StrStoreMap statsStrIdMap;
};

/**
 *  \class SedaStatsStore class is used to store the stats collected in 
 *         SedaStatsStage
 *
 *  \brief  Store and retrieve the stats sent to the sedastatsstore
 */
class SedaStatsStore{
    public:

        //! Constructor for class storing stats sent
        /**
         *  @brief              Initialize the members and save the statid
         *  @param[in] statId   Unique Id sent
         *  @return 
         */
        SedaStatsStore() :
            _persistent(false)
        {
            reset();
        }

        SedaStatsStore( bool persistent ):
            _persistent(persistent)
        {
            reset();
        }

        //! Destructor for class
        /**
         *  @brief              Nothing to do
         *  @param  
         *  @return 
         */
        ~SedaStatsStore(){};

        //! Calculate the average and return result
        /**
         *  @brief calculate the avg and return the result
         *  @return Average time calculated
         */
        unsigned long long  
            getAvgTime()        
        { 
            if(_numEffectiveStat > 0)
                return _totalTime / _numEffectiveStat;
            return 0;
        }


        //! Calculate the average and return result
        /**
         *  @brief calculate the avg and return the result
         *  @return Average counter value calculated
         */
        long long int
            getAvgIncrement()       
        {   return _avgIncrement;}
        long long int
            getMinIncrement()       
        {   return _minIncrement;}
        long long int
            getMaxIncrement()       
        {   return _maxIncrement;}


        //! Is this used to store counter values
        /**
         *  @return return true if counter value stored
         */
        bool
            isCounter()         {   return  _isCounter                  ;}

        //! Return the total of all the time differences sent to the stage
        /**
         *  @brief Return total time. 
         *          Time sent down to the stage is added and saved here
         *  @return Total Time
         */
        unsigned long long  
            getTotalTime()      
            {   
                return (_numEffectiveStat == _numStat) ? 
                    _totalTime : (unsigned long long)-1;
            }

        //! Get the number of stats sent down
        /**
         *  @brief  Return the number of pure time stats recorded
         *  @return Return _numStat
         */
        unsigned long long  
            getNumStats()       {   return  _numStat   ;}

        //! Return the Max value for time
        /**
         *  @brief  Return the max value of time recorded
         *  @return _minTime
         */
        unsigned long long  
            getMaxTime()        {   return  _maxTime        ;}

        //! Return the Min value for time
        /**
         *  @brief Return the min value of time recorded
         *  @return _minTime
         */
        unsigned long long  
            getMinTime()        {   return  _minTime        ;}

        //! Increment the total time with the time sent
        /**
         *  @brief  Collect a total of all time values sent
         *  @param  time    Time reported to statsStage
         */
        void 
            incTotalTime(unsigned long long time)   
        {
            if( _totalTime + time < _totalTime)
            {
                //Given a 64-core CPU, and average concurrent request number
                //1000, it takes around
                //   2^64/(10^6 us * 3600 * 24 * 365 * 64 * 1000) = 9.14 year
                //to reach here

                //_totalTime overflows, reset. 
                _totalTime = time;
                _numEffectiveStat = 1;
            }
            else
            {
                _totalTime  = _totalTime + time;
                _numEffectiveStat++;
            }

            _numStat++;
            
            if (_noStatReceived)    {   _minTime    =   time    ;
                                        _noStatReceived = false ;}
            if (_maxTime < time)    {   _maxTime    =   time    ;}  else
            if (_minTime > time)    {   _minTime    =   time    ;}
        }

        //! Return the total of all counter values reported
        /**
         *  @brief  Return the total of all counter values reported
         *  @return _totalCounter    
         */
        long long  
            getTotalCounter()   {   return  _totalCounter   ;}

        //! Return the count of counter values reported
        /**
         *  @brief A separate count of counters sent is kept. This is to 
         *          avoid the case where the same stat id is used to report
         *          pure time stats with no counter values and counter values 
         *          as well. If we use the count of time stats reported to 
         *          calculate the counter avg under such conditions 
         *          then wrong values will get reported if two separate 
         *          counters are not kept.
         *  @return _counterCount
         */
        unsigned long long  
            getCounterCount()   {   return  _counterCount   ;}


        //! Increment the total counter with the count sent in
        /**
         *  @brief This class maintains a sum of all counters measured 
         *          for the unique stat id.
         *  @param  count   count sent in to the stage
         */
        void
            incCounter  ( long long int count)  
            { 
                _totalCounter   +=   count  ;   
                _counterCount   ++          ;
                _isCounter      =   true    ;
                // calculate counter statistics
                _avgIncrement     = _totalCounter/_counterCount;
                if (true == _noCounterReceived) {
                    _minIncrement = count;
                    _maxIncrement = count;
                    _noCounterReceived = false;
                } else {
                    if (count < _minIncrement) _minIncrement = count;
                    if (count > _maxIncrement) _maxIncrement = count;
                } 
            }
        void
            resetCounter ( long long int count)
            {
                _totalCounter   =   count;
                _counterCount   =   1;
                _isCounter      =   true;

                _noCounterReceived =   false;
                _minIncrement     =   count;
                _avgIncrement     =   count;
                _maxIncrement     =   count;
            }

        //! Reset all members
        void 
            reset()
            {
                _numEffectiveStat   =   0;
                _totalTime      =   0;
                _numStat        =   0;
                _maxTime        =   0;
                _minTime        =   0;
                _isCounter      =   false;
                _totalCounter   =   0;
                _counterCount   =   0;
                _noStatReceived =   true;

                _noCounterReceived =   true;
                _minIncrement     =   0;
                _avgIncrement     =   0;
                _maxIncrement     =   0;
            }

        //! Check to see if this stat is persistent
        /**
         *  @brief This flag is set if the user does not wish to have the
         *         stat cleared
         *  @return true if the stat is set to be persistent
         */
        bool
            isPersistent(){ return _persistent;}

    private:
        unsigned long long  _totalTime; /**< \brief Total of all time sent  */
        unsigned long long  _maxTime;   /**< \brief Max time sent           */
        unsigned long long  _minTime;   /**< \brief Min time sent */
        unsigned long long  _numStat;   /**< \brief Total no sent to stage  */
        unsigned long long  _numEffectiveStat; /**< \brief Num of stat for calculating average time */
        bool                _noStatReceived;

        bool                _isCounter; /**< \brief is counter values stored*/
        //Counter related variables
        long long int _totalCounter; /**< \brief sum total Counter value sent   */
        bool          _noCounterReceived;
        long long int _avgIncrement; /**< \brief avg Counter value sent   */
        long long int _minIncrement; /**< \brief min Counter value sent   */
        long long int _maxIncrement; /**< \brief max Counter value sent   */

        //A separate counter count is maintained, if a stat id is used for 
        //collecting both only timings and counter values, the to know how many
        //increments or counter values have been sent to the stats stage a
        //different counter is needed
        unsigned long long  _counterCount; /**< \brief No of counters sent  */

        /** Flag to indicate if the stat is persistent */
        bool                _persistent;
};


#endif
