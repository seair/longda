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


#ifndef _KILLTHREAD_HXX_
#define _KILLTHREAD_HXX_


#include <list>

#include "defs.h"

#include "seda/stage.h"

/** 
 *  @file
 *  @author Longda
 *  @date   3/16/07
 */

class Threadpool;

//! A Stage to kill threads in a thread pool
/** 
 * The KillThreadStage is scheduled on a thread pool whenever threads
 * need to be killed.  Each event handled by the stage results in the
 * death of the thread.
 */
class KillThreadStage : public Stage {

public:

    //! parse properties, instantiate a summation stage object
    /**
     * @pre class members are uninitialized
     * @post initializing the class members
     * @return Stage instantiated object
     */
    static Stage* makeStage(const std::string& tag);

protected:

    //! Constructor
    /**
     * @param[in] tag     The label that identifies this stage.
     *
     * @pre  tag is non-null and points to null-terminated string
     * @post event queue is empty
     * @post stage is not connected
     */
    KillThreadStage(const char* tag) :
        Stage(tag)
        {}

    //! Notify the pool and kill the thread
    /**
     * @param[in] event Pointer to event that must be handled.
     * 
     * @post  Call never returns.  Thread is killed.  Pool is notified.
     */
    void handleEvent(StageEvent* event);

    //! Handle the callback
    /**
     * Nothing special for callbacks in this stage.
     */
    void callbackEvent(StageEvent* event, CallbackContext* context)
        { return; }
    
    //! Initialize stage params
    /**
     * Ignores nextStageList---there are no outputs for this stage.
     *
     * @pre  Stage not connected
     * @return true
     */
    bool    initialize()
        { return true; }

    //! set properties for this object
    /**
     * @pre class members are uninitialized
     * @post initializing the class members
     * @return Stage instantiated object
     */
    bool setProperties();

    friend class Threadpool;
};

#endif // _KILLTHREAD_HXX_

