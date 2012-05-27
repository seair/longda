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


#ifndef _SEDA_STATS_H_
#define _SEDA_STATS_H_


#include "seda/stage.h"
#include "seda/callback.h"

#define MAX_NUM_CATEGORY 2000
#define MAX_NUM_STATS    2000
#define ID_STR(ID,STR)          \
            case(ID):           \
                return(STR)
/**
 *  \class Describes the stat
 *  \brief
 *   
 *  \par 
 */
class SedaStats
{
    public:

        /// @brief Enumeration of all the category of stats
        //which are being collected
        //!! Also list a description of the category
        //in categoryStr() method!!
        typedef enum _SEDA_STATS_CATEGORY{
            //Used to address all categories
            ALL_CATEGORY            =    0,
            
            SERVER_JOB              =    100,

            CLIENT_JOB              =    200,

            NET_LATENCY_CAT         =    300,
            SVC_LATENCY_CAT         =    400,
            COMM_CAT                =    500,
            SVC_CAT                 =    600,
            
            //The upper limit for the max number of categories
            //Currently set at 4000
            DUMMY_END_CATEGORY      =   MAX_NUM_CATEGORY

        } sedaStatsCategory_t;

        //! Convert the category id to string
        /**
         *  @brief To help report the category being collected, routine to convert
         *  category to a descriptive string
         *  @param[in]  category  category of stat collected
         *  @return     The description as a string
         */
        static std::string categoryStr(int category){
            switch(category){
                ID_STR( ALL_CATEGORY    ,"All the categories listed");
                ID_STR( SERVER_JOB      ,"Command Processing stats for Server");
                ID_STR( CLIENT_JOB      ,"Command Processing stats for client");
                ID_STR( NET_LATENCY_CAT ,"Network Latency" );
                ID_STR( SVC_LATENCY_CAT ,"Service Latency" );
                ID_STR( COMM_CAT        ,"CommStage" );
                ID_STR( SVC_CAT         ,"Service" );
                ID_STR( DUMMY_END_CATEGORY
                                      ,"Dummy category, not a valid category!");
                default:
                {
                    return "NOT LISTED IN SEDASTATS.H!!!";
                }
            }
        }


        /// @brief Enumeration of all the stats which are being collected
        //!!! Also list the description of the stat in getIdStr() method!!!
        typedef enum _SEDA_STATS_ID{
            //Enter the stats you wish to collect to be able to
            //identify them uniquely

            //Used to address all stats
            ALL_STATS               =   0,
            //Miscellaneous 1-99 

            //Server 100 - 199
            SERVER_CREATE_OBJ          =   100,
            SERVER_NS_CREATE_OBJ       =   101,

            //Client 200 - 219
            CLIENT_CREATE_OBJ          =   200,
            CLIENT_NS_CREATE_OBJ       =   201,

            //Net 300 -400
            NET_LATENCY_STAT           =   300,
            RPC_HEADER_STAT            =   301,
            RPC_DESERIALIZE_MSG_STAT  =    302,
            RPC_MSG_REQ_STAT           =   303,
            RPC_MSG_RSP_STAT           =   304,


            DUMMY_END_STAT             =  MAX_NUM_STATS

        } sedaStatsIdentifier_t;

        //! Convert the stat id to string
        /**
         *  @brief To help report the stat being collected, routine to convert
         *  statid to string
         *  @param[in]  statId  Id of stat collected
         *  @return     The description as a string
         */
        static std::string getIdStr(int statId){
            switch(statId){
                ID_STR( SERVER_CREATE_OBJ       ,"Server Object Create");
                ID_STR( SERVER_NS_CREATE_OBJ    ,"Server create an object in NS");
                ID_STR( CLIENT_CREATE_OBJ       ,"Client create an object");
                ID_STR( CLIENT_NS_CREATE_OBJ    ,"Client Open an object");
                ID_STR( NET_LATENCY_STAT        ,"Network latency");
                ID_STR( RPC_HEADER_STAT         ,"RPC Header latency");
                ID_STR( RPC_DESERIALIZE_MSG_STAT,"RPC deserialize message");
                ID_STR( RPC_MSG_REQ_STAT        ,"RPC MSG latency");
                ID_STR( RPC_MSG_RSP_STAT        ,"RPC RSP latency");
                default:
                {
                    return "NOT LISTED IN SEDASTATS.H!!!";
                }
            }
        }

        // @ Identifier of stats, real type of it can be either 
        //   SedaStats::sedaStatsIdentifier_t or std::string
        class StatsId
        {
            public:
                enum Type {ID, STR, ALL,};
                Type                              type;
                SedaStats::sedaStatsIdentifier_t  id;
                std::string                       strId;

                StatsId() : type(ALL) {}
                StatsId(sedaStatsIdentifier_t id) : type(ID), id(id){}
                StatsId(const std::string& strId) : type(STR), strId(strId){}
        };

        /// @brief Destructor. Records the time it was called and sends the
        //         difference between create and delete to the stats 
        //         collection stage.
        ~SedaStats();

        /// @brief Constructor, it collects the stats identifier
        //         and records the time it was created if collectLogs
        //         is set to true
        /// 
        /// @param[in] identifier Identify uniquely the stat to be collected
        //  @param[in] category   Category for the event  
        /// @param[in] debugFlag  This flag will override the global
        //                        setting for stats logging. This 
        //                        can be used to report only one stat
        //                        selectively
        SedaStats(sedaStatsCategory_t   category,
                  sedaStatsIdentifier_t identifier, 
                  bool persistent=false);

        SedaStats(sedaStatsCategory_t category,
                  const std::string&  strId,
                  bool persistent=false);

        //! Get Stat category
        sedaStatsCategory_t getCategory() const { return _category; }

        //! Get Stat id
        const StatsId&  getStatId() const { return _statId; }

        /**
         *  @brief Submit a counting event to the stats collection stage.
         *
         *  This static method supports instances when a developer
         *  wants to record only the a count for a statistic.  The
         *  latency associated with a counting event will always be
         *  zero.  Using this static method avoids the memory
         *  management overhead of the approach that also records
         *  latency.
         *
         *  @param[in] identifier
         *    A unique identifier for the stat.
         *  @param[in] category
         *    The category of the event.
         *  @param[in] count
         *    The (possibly negative) quantity to add to the
         *    cumulative count.  If \c count is not specified, a
         *    default value of \c 1 is used.
         *  @param[in] persistent
         *    A flag determining if the can be reset by the \c
         *    SedaStatsEvent::resetCount flag.  If \c true, the
         *    statistic will not be cleared even if all statistics are
         *    reset.  If \c false, the statistic will be cleared with
         *    each reset.  If \c persistent is not specified, a
         *    default value of \c false is used.
         */
        static void recordCount(sedaStatsCategory_t category,
                                const StatsId&      statsId,
                                int count = 1,
                                bool persistent = false);

        /// @brief Record a count as a part of the stat. This is optional
        /// 
        /// @param[in] count the count which needs to be recorded
        void setCount(int count);

        //! Increment the counter value
        /**
         *  @brief If count is being collected then call this method
         *          to increment the counter
         */
        void incCount();

        //! Decrement the counter value
        /**
         *  @brief If count is being collected then call this method
         *          to decrement the counter
         */
        void decCount();


        //! Reset the count and set it to the value passed
        /**
         *  @brief If a count needs to be reset. Set the new value using this
         *          method
         *  @param  count   the new count
         *  @return none
         */
        void resetCount(int count);


        //! Abandon this stat
        /**
         *  @post no stat event will be sent when this \c sedaStats is destroyed
         */
        void abandon() { _enabled = false; }

    private:
        /// @brief The category being collected when this object is 
        //         instantiated. This is used to identify the category.
        sedaStatsCategory_t _category;

        /// @brief The unique stats being collected when this object is 
        //          instantiated. This is used to identify the stat.
        StatsId             _statId;
        
        struct timeval _startTime;  /**< \brief Stores time of creation */
        struct timeval _endTime;    /**< \brief Stores time of deletion */

        bool    _isCounter;    /**< \brief Is this object collecting a count */
        int     _statCount;    /**< \brief Store a count, this is optional */
        bool    _resetCount;   /**< \brief The counter is set with a new value*/

        //The stat does not persist over restarts of stage
        bool    _enabled;      /**< \brief Is statistics collection enabled */
        bool    _persistent;   /**< \brief Mark the stat as persistent*/


};

//! Class to accumulate the entire route time of a storage server event
class StatCbCtx: public CallbackContext
{
public:
    SedaStats* stat;
    StatCbCtx(SedaStats * pStat) : stat(pStat){}
    ~StatCbCtx() { delete stat; }
    void clearStat() { delete stat; stat = NULL; }
};

#endif
