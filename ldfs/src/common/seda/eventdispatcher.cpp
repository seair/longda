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


// Include Files
#include "eventdispatcher.h"

/**
 *  @file   Event Dispatcher
 *  @author Longda
 *  @date   8/20/07
 *
 *  Implementation of EventDispatcher class
 */


//! Constructor
EventDispatcher::EventDispatcher(const char* tag) :
    Stage(tag),
    eventStore(),
    nextStage(NULL)
{
    LOG_TRACE( "enter\n");

    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&eventLock, &attr);

    LOG_TRACE( "exit\n");
}


//! Destructor
EventDispatcher::~EventDispatcher()
{
    LOG_TRACE( "enter\n");
    pthread_mutex_destroy(&eventLock);
    LOG_TRACE( "exit\n");
}


//! Process an event
/**
 * Check if the event can be dispatched. If not, hash it and store
 * it.  If so, send it on to the next stage.
 */
void
EventDispatcher::handleEvent(StageEvent* event)
{
    LOG_TRACE( "enter\n");

    std::string      hash;
    DispatchContext* ctx = NULL;
    status_t         stat;

    pthread_mutex_lock(&eventLock);
    stat = dispatchEvent(event, ctx, hash);
    if (stat == SEND_EVENT) {
        nextStage->addEvent(event);
    }
    else if (stat == STORE_EVENT) {
        StoredEvent se(event, ctx);

        eventStore[hash].push_back(se);
    }
    else {
        LOG_ERROR("Dispatch event failure\n");
        // in this case, dispatchEvent is assumed to have disposed of event
    }
    pthread_mutex_unlock(&eventLock);

    LOG_TRACE( "exit\n");
}


//! Initialize stage params and validate outputs
bool
EventDispatcher::initialize()
{
    bool retVal = true;

    if (nextStageList.size() != 1) {
        retVal = false;
    }
    else {
        nextStage = *(nextStageList.begin());
    }
    return retVal;
}


//! Cleanup stage after disconnection
/**
 * Call done() on any events left over in the eventStore.
 */
void
EventDispatcher::cleanup()
{
    pthread_mutex_lock(&eventLock);

    // for each hash chain...
    for (EventHash::iterator i = eventStore.begin();
         i != eventStore.end();
         i++) {

        // for each event on the chain
        for (std::list<StoredEvent>::iterator j = i->second.begin();
             j != i->second.end();
             j++) {
            j->first->done();
        }
        i->second.clear();
    }
    eventStore.clear();

    pthread_mutex_unlock(&eventLock);
}


//! Wake up a stored event
bool
EventDispatcher::wakeupEvent(std::string hashkey)
{
    bool                sent = false;
    EventHash::iterator i;
    
    i = eventStore.find(hashkey);
    if (i != eventStore.end()) {

        // find the event and remove it from the current queue
        StoredEvent targetEv = *(i->second.begin());
        i->second.pop_front();
        if (i->second.size() == 0) {
            eventStore.erase(i);
        }

        // try to dispatch the event again
        status_t stat = dispatchEvent(targetEv.first,
                                      targetEv.second,
                                      hashkey);
        if (stat == SEND_EVENT) {
            nextStage->addEvent(targetEv.first);
            sent = true;
        }
        else if (stat == STORE_EVENT) {
            eventStore[hashkey].push_back(targetEv);
        }
        else {
            LOG_ERROR("Dispatch event failure\n");
            // in this case, dispatchEvent is assumed to have disposed of event
        }
    }

    return sent;
}

