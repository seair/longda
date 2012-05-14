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

/**
 * @ author: Longda Feng
 * @ date:  2010/08/13
 * @ func:  wrapper lock file
 */

#ifndef _DEBUG_MUTEX_HXX_
#define _DEBUG_MUTEX_HXX_

#include <pthread.h>
#include <string>
#include <map>


class CLockTrace
{
public:
    static void check (pthread_mutex_t *mutex, const int threadId, const char *file, const int line);
    static void lock (pthread_mutex_t *mutex, const int threadId, const char *file, const int line);
    static void tryLock(pthread_mutex_t *mutex, const int threadId, const char *file, const int line);
    static void unlock(pthread_mutex_t *mutex);

    static void toString(std::string& result);

    class CLockID
        {
        public:
            CLockID(const int threadId, const char *file, const int line):
                mFile(file),
                mThreadId(threadId),
                mLine(line)
            {

            }

            std::string toString()
            {
                std::string result;

                result += "threaId:" + mThreadId;
                result += ",file name:" + mFile;
                result += ",line:" + mLine;

                return result;
            }
        private:
            std::string mFile;
            int mThreadId;
            int mLine;
        };
protected:

    static std::map<pthread_mutex_t *, CLockID> mLocks;
};

//Open this macro in Makefile
#ifndef DEBUG_LOCK

#define MUTEXT_STATIC_INIT()     PTHREAD_MUTEX_INITIALIZER            
#define MUTEX_INIT(lock, attr)   pthread_mutex_init(lock, attr)
#define MUTEX_DESTROY(lock)      pthread_mutex_destroy(lock)
#define MUTEX_LOCK(lock)         pthread_mutex_lock(lock)
#define MUTEX_UNLOCK(lock)       pthread_mutex_unlock(lock)
#define MUTEX_TRYLOCK(lock)      pthread_mutex_trylock(lock)


#define COND_INIT(cond, attr)    pthread_cond_init(cond, attr)
#define COND_DESTROY(cond)       pthread_cond_destroy(cond)
#define COND_WAIT(cond, mutex)   pthread_cond_wait(cond, mutex)
#define COND_WAIT_TIMEOUT(cond, mutex, time, ret)  \
    ret = pthread_cond_timedwait(cond, mutex, time)
#define COND_SIGNAL(cond)        pthread_cond_signal(cond)
#define COND_BRAODCAST(cond)     pthread_cond_broadcast(cond) 


#else //DEBUG_LOCK
#include <sys/types.h>
#include "trace/log.h"


#define MUTEX_STATIC_INIT()                                       \
PTHREAD_MUTEX_INITIALIZER;                                         \
LOG_INFO("PTHREAD_MUTEX_INITIALIZER");

#define MUTEX_INIT(lock, attr)                                     \
({                                                                 \
    LOG_INFO("pthread_mutex_init");                                \
    int result = pthread_mutex_init(lock, attr);                   \
    result;                                                        \
})

#define MUTEX_DESTROY(lock)                                        \
({                                                                 \
    int result = pthread_mutex_destroy(lock);                      \
    LOG_INFO("pthread_mutex_destroy");                             \
    result;                                                        \
})

#define MUTEX_LOCK(mutex)                                          \
({                                                                 \
    CLockTrace::check(mutex, (int)gettid(), __FILE__, __LINE__);   \
    int result = pthread_mutex_lock(mutex);                        \
    CLockTrace::lock(mutex, (int)gettid(), __FILE__, __LINE__);    \
    result;                                                        \
})

#define MUTEX_TRYLOCK(mutex)                                       \
({                                                                 \
    CLockTrace::check(mutex, (int)gettid(), __FILE__, __LINE__);   \
    int result = pthread_mutex_trylock(mutex);                     \
    if (result == 0)                                               \
    {                                                              \
        CLockTrace::lock(mutex, (int)gettid(), __FILE__, __LINE__);\
    }                                                              \
    result;                                                        \
})

#define MUTEX_UNLOCK(lock)                                         \
({                                                                 \
    int result = pthread_mutex_unlock(lock);                       \
    CLockTrace::unlock(lock);                                      \
    LOG_INFO("mutex:%p has been ulocked", lock);                   \
    result;                                                        \
})

#define COND_INIT(cond, attr)                                      \
({                                                                 \
    LOG_INFO("pthread_cond_init");                                 \
    int result = pthread_cond_init(cond, attr);                    \
    result;                                                        \
})

#define COND_DESTROY(cond)                                         \
({                                                                 \
    int result = pthread_cond_destroy(cond );                      \
    LOG_INFO("pthread_cond_destroy");                              \
    result ;                                                       \
})

#define COND_WAIT(cond, mutex)                                     \
({                                                                 \
    LOG_INFO("pthread_cond_wait, cond:%p, mutex:%p", cond, mutex); \
    CLockTrace::unlock(mutex);   \
    int result = pthread_cond_wait(cond, mutex);                       \
    CLockTrace::lock(mutex, (int)gettid(), __FILE__, __LINE__);     \
    LOG_INFO("Lock %p under pthread_cond_wait", mutex);            \
    result ;                                                       \
})

#define COND_WAIT_TIMEOUT(cond, mutex, time, ret)                  \
({                                                                 \
    LOG_INFO("pthread_cond_timedwait, cond:%p, mutex:%p", cond, mutex); \
    CLockTrace::unlock(mutex);   \
    int result = pthread_cond_timedwait(cond, mutex, time);        \
    if (result == 0)                                               \
    {                                                              \
        CLockTrace::lock(mutex, (int)gettid(), __FILE__, __LINE__); \
        LOG_INFO("Lock %p under pthread_cond_wait", mutex);        \
    }                                                              \
    result;                                                        \
})
    
#define COND_SIGNAL(cond)                                          \
({                                                                 \
    int result = pthread_cond_signal(cond);                        \
    LOG_INFO("pthread_cond_signal, cond:%p", cond);                \
    result ;                                                       \
})

#define COND_BRAODCAST(cond)                                       \
({                                                                 \
    int result = pthread_cond_broadcast(cond);                     \
    LOG_INFO("pthread_cond_broadcast, cond:%p", cond);             \
    result;                                                        \
})


#endif //DEBUG_LOCK


#endif // _DEBUG_MUTEX_HXX__

