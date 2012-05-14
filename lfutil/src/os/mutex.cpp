// __CR__
// Copyright (c) 2008-2011 Longda Corporation
// All Rights Reserved
//
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which
// it is provided by or on behalf of Longda.
// __CR__

/**
 * @ author: hustjackie@gmail.com
 * @ date:  2010/04/01
 * @ func:  provide project common log functions
 */

#include "os/mutex.h"
#include "trace/log.h"

std::map<pthread_mutex_t *, CLockTrace::CLockID> CLockTrace::mLocks;

void CLockTrace::check(pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    LOG_INFO("Lock mutex %p, %s:%d", mutex, file, line);
    if (mLocks.find(mutex) != mLocks.end())
    {
        LOG_INFO("mutex %p has been lock already", mutex, file, line);
        return;
    }
}

void CLockTrace::lock (pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    CLockID  lockID(threadId, file, line);

    mLocks.insert(std::pair<pthread_mutex_t *, CLockID>(mutex, lockID));
}

void CLockTrace::tryLock(pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    if (mLocks.find(mutex) != mLocks.end())
    {
        return ;
    }

    CLockID lockId(threadId, file, line);
    mLocks.insert(std::pair<pthread_mutex_t *, CLockID>(mutex, lockId));
}

void CLockTrace::unlock(pthread_mutex_t *mutex)
{
    if (mLocks.find(mutex)  == mLocks.end())
    {
        LOG_ERROR("mutex:%p has already been unlocked", mutex);
        return ;
    }

    mLocks.erase(mutex);

}

void CLockTrace::toString(std::string& result)
{

    for(std::map<pthread_mutex_t *, CLockID>::iterator it = mLocks.begin();
            it != mLocks.end(); it++)
    {
        result += it->second.toString();

        char pointerBuf[24] = {0};
        sprintf(pointerBuf, ",mutex:%p\n", it->first);

        result += pointerBuf;
    }

    return ;
}
