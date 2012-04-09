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



#ifndef __CLIENT_EVENT__
#define __CLIENT_EVENT__

#include <semaphore.h>
#include <set>
#include <memory>

#include "mutex.h"
#include "stageevent.h"

#include "metadata.h"

class ClientEvent : public StageEvent
{
public:
    ClientEvent():
        mError(0)
    {
        sem_init(&mSem, 0, 0);
    }
        
    ~ClientEvent(){sem_destroy(&mSem);};

    int WaitProcessing();

    void finish();

    virtual void * GetFinishData(){return NULL;}
private:
    sem_t       mSem;

public:
    int         mError;
    std::string mErrMsg;
    
};

class ClientTestEvent : public ClientEvent
{
public:
    ClientTestEvent(){}
    ~ClientTestEvent(){}
    
};

class ClientDfsEvent : public ClientEvent
{
public:
    ClientDfsEvent(){MUTEX_INIT(&mLock, NULL);}
    ~ClientDfsEvent(){MUTEX_DESTROY(&mLock);}

    void CheckFinished(StageEvent *blockEvent)
    {
        int finished = false;

        MUTEX_LOCK(&mLock);
        
        mBlockEvents.erase((StageEvent *)blockEvent);
        
        if (mBlockEvents.empty())
        {
            finished = true;
        }
        MUTEX_UNLOCK(&mLock);

        if (finished == true)
        {
            done();
        }

        return ;
    }

    void * GetFinishData()
    {
        void * ret = (void *)mDG.get();

        mDG.release();

        return ret;
    }

public:
    pthread_mutex_t                 mLock;
    std::set<StageEvent *>          mBlockEvents;
    std::auto_ptr<DataGeography>    mDG;
};



class ClientDownloadEvent : public ClientDfsEvent
{
public:
    ClientDownloadEvent(const char *pFileName, 
        const u32_t type,
        const char *pBlockValue):
        mFileName(pFileName),
        mType(type),
        mBlockValue(pBlockValue)
        {}
    ~ClientDownloadEvent(){}

public:
    std::string                  mFileName;
    u32_t                        mType;
    std::string                  mBlockValue;
    
};

class ClientUploadEvent : public ClientDfsEvent
{
public:
    ClientUploadEvent(const char *pFileName):
        mFileName(pFileName)
    {
    }

public:
    std::string                          mFileName;
};

class ClientDeleteEvent: public ClientDfsEvent
{
public:
    ClientDeleteEvent(const u32_t type, const char *pId):
        mType(type),
        mBlockValue(pId)
    {
    }
public:
    u32_t                              mType;
    std::string                        mBlockValue;
};

int  HandleClientEvent(ClientEvent *event, char** finishData);
#endif //__CLIENT_EVENT__

