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

//Open this macro in Makefile
#ifndef DEBUG_LOCK_LOG

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
#include "log.h"


#define MUTEXT_STATIC_INIT()                     \
PTHREAD_MUTEX_INITIALIZER;                       \
LOG_INFO("PTHREAD_MUTEX_INITIALIZER");

#define MUTEX_INIT(lock, attr)                    \
({                                                \
    LOG_INFO("pthread_mutex_init");               \
    pthread_mutex_init(lock, attr);               \
})

#define MUTEX_DESTROY(lock)                      \
({                                               \
    pthread_mutex_destroy(lock);                 \
    LOG_INFO("pthread_mutex_destroy");           \
})

#define MUTEX_LOCK(lock)                         \
({                                               \
    LOG_INFO("pthread_mutex_lock");              \
    pthread_mutex_lock(lock);                    \
})

#define MUTEX_TRYLOCK(lock)                      \
({                                               \
    LOG_INFO("pthread_mutex_trylock");           \
    pthread_mutex_trylock(lock);                 \
})

#define MUTEX_UNLOCK(lock)                       \
({                                               \
    pthread_mutex_unlock(lock);                  \
    LOG_INFO("pthread_mutex_unlock");            \
})

#define COND_INIT(cond, attr)                    \
({                                               \
    LOG_INFO("pthread_cond_init");               \
    pthread_cond_init(cond, attr);               \
})

#define COND_DESTROY(cond)                       \
({                                               \
    pthread_cond_destroy(cond );                 \
    LOG_INFO("pthread_cond_destroy");            \
})

#define COND_WAIT(cond, mutex)                   \
({                                               \
    LOG_INFO("pthread_cond_wait");               \
    pthread_cond_wait(cond, mutex);              \
})

#define COND_WAIT_TIMEOUT(cond, mutex, time, ret)\
({                                               \
    LOG_INFO("pthread_cond_timedwait");          \
    ret = pthread_cond_timedwait(cond, mutex, time);   \
})
    
#define COND_SIGNAL(cond)                        \
({                                               \
    pthread_cond_signal(cond);                   \
    LOG_INFO("pthread_cond_signal");             \
})

#define COND_BRAODCAST(cond)                     \
({                                               \
    pthread_cond_broadcast(cond);                \
    LOG_INFO("pthread_cond_broadcast");          \
})


#endif //DEBUG_LOCK

#endif // _DEBUG_MUTEX_HXX__

