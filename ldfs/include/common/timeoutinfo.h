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


#ifndef _TIMEOUT_INFO_
#define _TIMEOUT_INFO_

#include <time.h>

#include "mutex.h"


//! Timeout info class used to judge if a certain deadline has reached or not.
/**
 * It's good to use handle-body to automate the reference count
 * increase/decrease. However, explicit attach/detach interfaces
 * are used here to simplify the implementation.
 */

class TimeoutInfo
{
public:

    //! Constructor
    /**
     * @param[in] deadline  deadline of this timeout
     */
    TimeoutInfo(time_t deadline);

    //! Increase ref count
    void attach();

    //! Decrease ref count
    void detach();

    //! Check if it has timed out
    bool hasTimedOut();

private:

    // Forbid copy ctor and =() to support ref count

    //! Copy constructor.
    TimeoutInfo(const TimeoutInfo& ti);

    //! Assignment operator.
    TimeoutInfo& operator=(const TimeoutInfo& ti);

protected:

    // Avoid heap-based \c TimeoutInfo
    // so it can easily associated with \c StageEvent

    //! Destructor.
    ~TimeoutInfo();

private:

    time_t deadline;        //!< when should this be timed out

    //!< used to predict timeout if now + reservedTime > deadline 
    //time_t reservedTime;

    bool isTimedOut;        //!< timeout flag

    int refCnt;             //!< reference count of this object
    pthread_mutex_t mutex;  //!< mutex to protect refCnt and flag
};

#endif // _TIMEOUT_INFO_
