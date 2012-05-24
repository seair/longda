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
std::map<pthread_mutex_t *, CLockTrace::CLockID> CLockTrace::mUnLocks;
std::map<pthread_mutex_t *, int>       CLockTrace::mWaitTimes;
std::map<int, pthread_mutex_t *> CLockTrace::mWaitLocks;
std::map<int, std::set<pthread_mutex_t *> > CLockTrace::mOwnLocks;

pthread_mutex_t CLockTrace::mMapMutex = PTHREAD_MUTEX_INITIALIZER;
int             CLockTrace::mMaxBlockTids = 8;

bool CLockTrace::deadlockCheck(pthread_mutex_t *mutex,
        const int threadId,
        const char *file,
        const int line)
{
    mWaitLocks[threadId] = mutex;

    std::map<pthread_mutex_t *, CLockTrace::CLockID>::iterator itLocks = mLocks.find(
            mutex);
    if (itLocks == mLocks.end())
    {
        return false;
    }
    CLockTrace::CLockID &other = itLocks->second;

    std::map<int, std::set<pthread_mutex_t *> >::iterator it = mOwnLocks.find(threadId);
    if (it == mOwnLocks.end())
    {
        return false;
    }
    std::set<pthread_mutex_t *> &ownMutexs = it->second;

    std::map<int, pthread_mutex_t *>::iterator otherIt = mWaitLocks.find(other.mThreadId);
    if (otherIt == mWaitLocks.end())
    {
        return false;
    }
    pthread_mutex_t *otherWaitMutex = otherIt->second;

    if (ownMutexs.find(otherWaitMutex) == ownMutexs.end())
    {
        //no dead lock
        return false;
    }
    else
    {

        std::map<pthread_mutex_t *, CLockTrace::CLockID>::iterator it =
                mLocks.find(otherWaitMutex);
        if (it != mLocks.end())
        {
            CLockID &ownLockId = it->second;
            LOG_ERROR("Thread %d own mutex %p:%s:%d and try to get %p:%s:%d, "
            "other thread %d own mutex %p:%s:%d and try to get %p",
                    threadId, otherWaitMutex, ownLockId.mFile.c_str(),
                    ownLockId.mLine, mutex, file, line,
                    other.mThreadId, mutex, other.mFile.c_str(), other.mLine,
                    otherWaitMutex);

            std::string output;
            toString(output);
            LOG_INFO("Locks information:%s", output.c_str());
        }
        else
        {
            LOG_WARN("Thread %d own empty %p mutex", threadId, otherWaitMutex);
        }


        return true;
    }

}

bool CLockTrace::checkAllThreadsBlock(pthread_mutex_t *mutex,
        const char *file,
        const int line)
{
    std::map<pthread_mutex_t *, int>::iterator it = mWaitTimes.find(mutex);
    if (it == mWaitTimes.end())
    {
        mWaitTimes.insert(std::pair<pthread_mutex_t *, int>(mutex, 1));

        return false;
    }

    int lockTimes = it->second;
    mWaitTimes[mutex] = lockTimes + 1;
    if (lockTimes >= mMaxBlockTids - 1)
    {

        //std::string          lastLockId = lockId.toString();
        CLockTrace::CLockID &lockId = mLocks[mutex];
        LOG_WARN("mutex %p has been already lock %d times, this time %s:%d, first time:%d:%s:%d",
                mutex, lockTimes, file, line,
                lockId.mThreadId, lockId.mFile.c_str(), lockId.mLine);

        std::string output;
        toString(output);
        LOG_INFO("Locks information:%s", output.c_str());
        return true;
    }
    else
    {
        return false;
    }
}

void CLockTrace::check(pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    MUTEX_LOG("Lock mutex %p, %s:%d", mutex, file, line);
    pthread_mutex_lock(&mMapMutex);

    deadlockCheck(mutex, threadId, file, line);

    checkAllThreadsBlock(mutex, file, line);

    pthread_mutex_unlock(&mMapMutex);
}

void CLockTrace::insertLock (pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    CLockID  lockID(threadId, file, line);

    mLocks.insert(std::pair<pthread_mutex_t *, CLockID>(mutex, lockID));

    mWaitLocks.erase(threadId);

    //add entry to mOwnLocks
    std::set<pthread_mutex_t *> &ownLockSet = mOwnLocks[threadId];
    ownLockSet.insert(mutex);

    std::map<pthread_mutex_t *, int>::iterator itTimes = mWaitTimes.find(mutex);
    if (itTimes == mWaitTimes.end())
    {
        LOG_ERROR("No entry of %p:%s:%d in mWaitTimes", mutex, file, line);

    }
    else
    {
        mWaitTimes[mutex] = itTimes->second -1;
    }
}

void CLockTrace::lock (pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    pthread_mutex_lock(&mMapMutex);

    insertLock (mutex, threadId, file, line);
    pthread_mutex_unlock(&mMapMutex);
}

void CLockTrace::tryLock(pthread_mutex_t *mutex, const int threadId, const char *file, const int line)
{
    pthread_mutex_lock(&mMapMutex);
    if (mLocks.find(mutex) != mLocks.end())
    {
        pthread_mutex_unlock(&mMapMutex);
        return ;
    }

    insertLock (mutex, threadId, file, line);
    pthread_mutex_unlock(&mMapMutex);
}

void CLockTrace::unlock(pthread_mutex_t *mutex, int threadId, const char *file, int line)
{
    pthread_mutex_lock(&mMapMutex);
    if (mLocks.find(mutex)  == mLocks.end() )
    {
        std::map<pthread_mutex_t *, CLockTrace::CLockID>::iterator it = mUnLocks.find(mutex);
        if (it != mUnLocks.end())
        {
            LOG_WARN("mutex:%p:%s:%d has already been unlocked, last unlock %s:%d",
                    mutex, file, line, it->second.mFile.c_str(), it->second.mLine);
        }
        else
        {
            LOG_WARN("mutex %p:%s:%d doesn't in mLock and mUnlocks", mutex, file, line);
        }
    }

    mLocks.erase(mutex);
    CLockID  lockID(threadId, file, line);
    mUnLocks[mutex] = lockID;

    std::set<pthread_mutex_t *> &ownLockSet = mOwnLocks[threadId];
    ownLockSet.erase(mutex);

    pthread_mutex_unlock(&mMapMutex);

}

void CLockTrace::toString(std::string& result)
{

    //pthread_mutex_lock(&mMapMutex);
    for(std::map<pthread_mutex_t *, CLockID>::iterator it = mLocks.begin();
            it != mLocks.end(); it++)
    {
        result += it->second.toString();

        char pointerBuf[24] = {0};
        sprintf(pointerBuf, ",mutex:%p\n", it->first);

        result += pointerBuf;
    }
    //pthread_mutex_unlock(&mMapMutex);

    return ;
}
