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


#ifndef _EVENTDISPATCHER_HXX_
#define _EVENTDISPATCHER_HXX_

// Include Files
#include <map>
#include <list>

// SEDA headers
#include "seda/stage.h"
#include "seda/stageevent.h"

/**
 *  @file   Event Dispatcher
 *  @author Longda
 *  @date   8/20/07
 */

class DispatchContext;

//! A stage which stores and re-orders events
/** 
 * The EventDispatcher stage is designed to assert control over the order
 * of events that move through the Seda pipeline.  The EventDispatcher stage
 * has a hash table that stores events for later processing.  As each event
 * arrives at the Dispatcher, a test is applied to determine whether it
 * should be allowed to proceed.  This test is implemented in subclasses
 * and uses state from the event and state held in the dispatcher itself.
 * If the event is meant to be delayed, it is hashed and stored.  The
 * dispatcher also provides an interface that "wakes up" a stored, event
 * and re-applies the dispatch test.  This wake-up interface can be called
 * from a background thread, or from a callback associated with an event
 * that has already been dispatched.
 *
 * The EventDispatcher class is designed to be specialized by adding a
 * specific implementation of the dispatch test for events, and a method 
 * or process for waking up stored events at the appropriate time.
 */

class EventDispatcher : public Stage {

    // public interface operations

public:

    typedef enum {
        SEND_EVENT = 0,
        STORE_EVENT,
        FAIL_EVENT
    } status_t;

    //! Destructor
    /**
     * @pre  stage is not connected
     * @post pending events are deleted and stage is destroyed
     */
    virtual ~EventDispatcher();

    //! Process an event
    /**
     * Check if the event can be dispatched. If not, hash it and store
     * it.  If so, send it on to the next stage
     *
     * @param[in] event Pointer to event that must be handled.
     * @post  event must not be de-referenced by caller after return
     */
    void handleEvent(StageEvent* event);

    // Note, EventDispatcher is an abstract class and needs no makeStage()

protected:

    //! Constructor
    /**
     * @param[in] tag     The label that identifies this stage.
     *
     * @pre  tag is non-null and points to null-terminated string
     * @post event queue is empty
     * @post stage is not connected
     */
    EventDispatcher(const char* tag);

    //! Initialize stage params and validate outputs
    /**
     * @pre  Stage not connected
     * @return TRUE if and only if outputs are valid and init succeeded.
     */
    bool initialize();

    //! set properties for this object
    bool setProperties() { return true; }

    //! Cleanup stage after disconnection
    /**
     * After disconnection is completed, cleanup any resources held by the
     * stage and prepare for destruction or re-initialization.
     */
    virtual void cleanup();

    //! Dispatch test
    /**
     * @param[in] ev  Pointer to event that should be tested
     * @param[in/out]  Pointer to context object
     * @param[out] hash  Hash value for event
     *
     * @pre eventLock is locked
     * @post hash is calculated if return val is false
     * @return SEND_EVENT if ok to send the event down the pipeline;
     *                    ctx is NULL
     *         STORE_EVENT if event should be stored; ctx will be stored
     *         FAIL_EVENT if failure, and event has been completed;
     *                    ctx is NULL
     */
    virtual status_t dispatchEvent(StageEvent* ev,
                                   DispatchContext*& ctx,
                                   std::string& hash) = 0;

    //! Wake up a stored event
    /**
     * @pre eventLock is locked
     * @return true if an event was found on hash-chain associated with
     *              hashkey and sent to next stage
     *         false no event was found on hash-chain
     */
    bool wakeupEvent(std::string hashkey);

    // implementation state

    typedef std::pair<StageEvent*, DispatchContext*> StoredEvent;
    typedef std::map<std::string, std::list<StoredEvent> > EventHash;

    EventHash       eventStore;   //!< events stored here while waiting
    pthread_mutex_t eventLock;    //!< protects access to eventStore
    Stage*          nextStage;    //!< target for dispatched events

protected:

};


/**
 * Class to store context info with the stored event.  Subclasses should
 * derive from this base class.
 */
class DispatchContext {
public:
    virtual ~DispatchContext() {}
};

#endif // _EVENTDISPATCHER_HXX_
