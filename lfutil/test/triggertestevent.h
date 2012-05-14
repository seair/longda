// __CR__
// Copyright (c) 2008-2012 LongdaFeng
// All Rights Reserved
// 
// This software contains the intellectual property of LongdaFeng
// or is licensed to LongdaFeng from third parties.  Use of this 
// software and the intellectual property contained therein is 
// expressly limited to the terms and conditions of the License Agreement  
// under which it is provided by or on behalf of LongdaFeng.
// __CR__


/*
 * triggertestevent.h
 *
 *  Created on: May 9, 2012
 *      Author: Longda Feng
 */

#ifndef TRIGGERTESTEVENT_H_
#define TRIGGERTESTEVENT_H_

#include "seda/stageevent.h"
#include "os/mutex.h"

class TriggerTestEvent : public StageEvent
{
public:
    TriggerTestEvent(int sleepTime):mSleepTime(sleepTime), mTimes(0){MUTEX_INIT(&mTestMutex, NULL);};
    ~TriggerTestEvent(){MUTEX_DESTROY(&mTestMutex);}

    int getSleepTime() const
    {
        return mSleepTime;
    }

    void setSleepTime(int sleepTime)
    {
        mSleepTime = sleepTime;
    }

public:
    int mSleepTime;
    int mTimes;
    pthread_mutex_t           mTestMutex;
};




#endif /* TRIGGERTESTEVENT_H_ */
