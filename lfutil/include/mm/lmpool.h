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
 * lmpool.h
 *
 *  Created on: Apr 27, 2012
 *      Author: Longda Feng
 */

#ifndef LMPOOL_H_
#define LMPOOL_H_

#include <queue>

#include "os/mutex.h"
#include "trace/log.h"

#define CLMPOOL_DEFAULT_ADDSIZE 16

template <class T>
class CLmpool
{
public:
    CLmpool():
        mQueue(),
        mAddSize(CLMPOOL_DEFAULT_ADDSIZE)
    {
        MUTEX_INIT(&mLock, NULL);
    }

    ~CLmpool()
    {
        MUTEX_LOCK(&mLock);
        while(mQueue.empty() == false)
        {
            T * item = mQueue.front();
            mQueue.pop();
            delete item;
        }

        MUTEX_UNLOCK(&mLock);
        MUTEX_DESTROY(&mLock);
    }

    int add(int addSize)
    {
        int ret = 0;

        MUTEX_LOCK(&mLock);
        for (int i = 0; i < addSize; i++)
        {
            T *item = new T();
            if (item == NULL)
            {
                ret = -1;
                break;
            }
            mQueue.push(item);
        }
        MUTEX_UNLOCK(&mLock);

        return ret;
    }

    int init(int initSize)
    {
        return add(initSize);
    }

    T* get()
    {
        T * ret = NULL;

        MUTEX_LOCK(&mLock);
        if (mQueue.empty() == true)
        {
            add(mAddSize);
        }

        if (mQueue.empty() == false)
        {
            ret = mQueue.front();
            mQueue.pop();
        }

        MUTEX_UNLOCK(&mLock);

        return ret;
    }

    void put(T *item)
    {
        MUTEX_LOCK(&mLock);

        mQueue.push(item);

        MUTEX_UNLOCK(&mLock);
    }


private:
    std::queue<T *>      mQueue;
    pthread_mutex_t      mLock;
    int                  mAddSize;

};


#endif /* LMPOOL_H_ */
