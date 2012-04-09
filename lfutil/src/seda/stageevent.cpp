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
#include <assert.h>
#include <stdlib.h>

#include "defs.h"
#include "linit.h"
#include "trace/log.h"
#include "os/mutex.h"

#include "seda/stageevent.h"
#include "seda/callback.h"
#include "time/timeoutinfo.h"


/** 
 * @author Longda
 * @date   3/27/07
 *
 * Implementation of StageEvent class.
 */


//! Constructor
StageEvent::StageEvent() : 
    compCB(NULL),
    ud(NULL),
    cbFlag(false),
    history(NULL),
    stageHops(0),
    tmInfo(NULL)
{

}


//! Destructor
StageEvent::~StageEvent()
{
    // clear all pending callbacks
    while (compCB) {
        CompletionCallback* top = compCB;
        compCB = compCB->popCallback();
        delete top;
    }

    // delete the history if present
    if (history) {
        history->clear();
        delete history;
    }

    if (tmInfo) {
        tmInfo->detach();
        tmInfo = NULL;
    }
}


//! Processing for this event is done; callbacks executed
void
StageEvent::done()
{
    CompletionCallback* top;

    if (compCB) {
        top = compCB;
        markCallback();
        top->eventReschedule(this);
    } else {
        delete this;
    }
}


//! Processing for this event is done; callbacks executed immediately
void
StageEvent::doneImmediate()
{
    CompletionCallback* top;

    if (compCB) {
        top = compCB;
        clearCallback();
        compCB = compCB->popCallback();
        top->eventDone(this);
        delete top;
    } else {
        delete this;
    }
}

void
StageEvent::doneTimeout()
{
    CompletionCallback* top;

    if (compCB) {
        top = compCB;
        clearCallback();
        compCB = compCB->popCallback();
        top->eventTimeout(this);
        delete top;
    } else {
        delete this;
    }
}

//! Push the completion callback onto the stack
void
StageEvent::pushCallback(CompletionCallback* cb)
{
    cb->pushCallback(compCB);
    compCB = cb;
}


void
StageEvent::setUserData(UserData *u)
{
    ud = u;
    return;
}

UserData*
StageEvent::getUserData()
{
    return ud;
}


//! Add stage to list of stages which have handled this event
void
StageEvent::saveStage(Stage* stg, HistType type)
{
    if (!history) {
        history = new std::list<HistEntry>;
    }
    if (history) {
        history->push_back(std::make_pair(stg, type));
        stageHops++;
        ASSERT(stageHops <= theMaxEventHops(), "Event exceeded max hops");
    }
}

void
StageEvent::setTimeoutInfo(TimeoutInfo* tmi)
{
    // release the previous timeout info
    if (tmInfo) {
        tmInfo->detach();
    }

    tmInfo = tmi;
    if (tmInfo) {
        tmInfo->attach();
    }
}

void
StageEvent::setTimeoutInfo(time_t deadline)
{
    TimeoutInfo* tmi = new TimeoutInfo(deadline);
    setTimeoutInfo(tmi);
}

void
StageEvent::setTimeoutInfo(const StageEvent& ev)
{
    setTimeoutInfo(ev.tmInfo);
}

bool
StageEvent::hasTimedOut()
{
    if (!tmInfo) {
        return false;
    }

    return tmInfo->hasTimedOut();
}

//! Accessor function which wraps value for max hops an event is allowed
u32_t&
theMaxEventHops() 
{
    static u32_t maxEventHops = 0;
    return maxEventHops;
}

//! Accessor function which wraps value for event history flag
bool&
theEventHistoryFlag()
{
    static bool eventHistoryFlag = false;
    return eventHistoryFlag;
}
