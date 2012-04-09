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


#ifndef _THREADPOOL_
#define _THREADPOOL_

#include <deque>

#include "defs.h"

#include "seda/killthread.h"

/** 
 *  @file
 *  @author Longda
 *  @date   3/13/07
 */

class Stage;

//! A thread pool for one or more seda stages
/** 
 * The Threadpool class consists of a pool of worker threads and a
 * scheduling queue of active seda Stages that have events that need
 * processing.  Each thread in the thread pool constantly examines the
 * head of the scheduling queue for a scheduled stage.  It then removes
 * the scheduled stage from the queue, and selects an event from the
 * Stage's event queue for processing. The thread then processes the event
 * using the Stage's handleEvent() member function before starting the
 * process over.  If the thread finds the scheduling queue empty, it sleeps
 * on a condition waiting for Stages to schedule themselves.
 * <p>
 * The number of threads in the pool can be controlled by clients. On
 * creation, the caller provides a parameter indicating the initial number
 * of worker threads, but this number can be adjusted at any time by using
 * the addThreads(), numThreads(), and killThreads() interfaces.
 */
class Threadpool {

public:

    //! Constructor
    /**
     * @param[in] threads The number of threads to create.
     * @param[in] name    Name of the thread pool.
     *
     * @post thread pool has <i>threads</i> threads running
     */
    Threadpool(unsigned int threads, const std::string& name = std::string());

    //! Destructor
    /**
     * Kills all threads and destroys pool.
     * 
     * @post all threads are destroyed and pool is destroyed
     */
    virtual ~Threadpool();

    //! Query number of threads.
    /**
     * @return number of threads in the thread pool.
     */
    unsigned int numThreads();
  
    //! Add threads to the pool
    /**
     * @param[in] threads Number of threads to add to the pool.
     *
     * @post  0 <= (# of threads in pool) - (original # of threads in pool)
     *        <= threads
     * @return number of thread successfully created
     */
    unsigned int addThreads(unsigned int threads);
  
    //! Kill threads in pool
    /**
     * Blocks until the requested number of threads are killed.  Won't
     * kill more than current number of threads.
     *
     * @param[in] threads Number of threads to kill.
     *
     * @post (original # of threads in pool) - (# of threads in pool)
     *       <= threads
     * @return number of threads successfully killed.
     */
    unsigned int killThreads(unsigned int threads);

    //! Schedule stage with some work
    /**
     * Schedule a stage with some work to be done on the run queue.
     *
     * @param[in] stageP Reference to stage to be scheduled.
     * 
     * @pre  stageP must have a non-empty queue.
     * @post stageP is scheduled on the run queue.
     */
    void schedule(Stage* stageP);

    //! Get name of thread pool
    const std::string& getName();

    //! Initialize the static data structures of ThreadPool
    static void createPoolKey();

    //! Finalize the static data structures of ThreadPool
    static void delPoolKey();

protected:

    //! Internal thread kill.
    /**
     * Internal operation called only when a thread kill event is processed.
     * Reduces the count of active threads, and, if this is the last pending 
     * kill, signals the waiting killThreads method.
     */
    void threadKill();    

    //! Internal generate kill thread events
    /**
     * Internal operation called by killThreads(). Generates the requested
     * number of kill thread events and schedules them.
     * 
     * @pre  thread mutex is locked.
     * @pre  toKill <= current number of threads
     * @return number of kill thread events successfully scheduled
     */
    unsigned int genKillThreadEvents(unsigned int toKill);

private:

    //! Internal thread control function
    /**
     * Function which contains the control loop for each service thread.
     * Should not be called except when a thread is created.
     */
    static void * runThread(void* poolPtr);

    //! Save the thread pool pointer for this thread
    static void setThreadPoolPtr(const Threadpool* thdPool);

    //! Get the thread pool pointer for this thread
    static const Threadpool* getThreadPoolPtr();

private:

    // run queue state
    pthread_mutex_t    runMutex;   //!< protects the run queue
    pthread_cond_t     runCond;    //!< wait here for stage to be scheduled
    std::deque<Stage*> runQueue;   //!< list of stages with work to do
    bool               eventhist;  //!< is event history enabled?

    // thread state
    pthread_mutex_t threadMutex;       //!< protects thread state
    pthread_cond_t  threadCond;        //!< wait here when killing threads
    unsigned int    nthreads;          //!< number of service threads
    unsigned int    threadsToKill;     //!< number of pending kill events
    unsigned int    nIdles;            //!< number of idle threads
    KillThreadStage killer;            //!< used to kill threads
    std::string     name;              //!< name of threadpool

    //! key of thread specific to store thread pool pointer
    static pthread_key_t poolPtrKey;

    // allow KillThreadStage to kill threads
    friend class KillThreadStage;
};

#endif // _THREADPOOL_
