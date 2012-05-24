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
 * CTestStage.cpp
 *
 *  Created on: Apr 5, 2012
 *      Author: Longda Feng
 */
#include <string>
#include <string.h>

#include "linit.h"
#include "conf/ini.h"
#include "lang/lstring.h"
#include "seda/timerstage.h"
#include "io/io.h"

#include "comm/commevent.h"
#include "comm/request.h"
#include "comm/response.h"

#include "teststage.h"
#include "triggertestevent.h"


#define ONE_TIME_NUM 1000

//! Constructor
CTestStage::CTestStage(const char* tag) :
        Stage(tag),
        mTimerStage(NULL),
        mCommStage(NULL),
        mTestTimes(0),
        mSendCounter(0),
        mLastSendCounter(0),
        mRecvCounter(0),
        mLastRecvCounter(0)
{
    MUTEX_INIT(&mSendMutex, NULL);
    MUTEX_INIT(&mRecvMutex, NULL);
}

//! Destructor
CTestStage::~CTestStage()
{
    MUTEX_DESTROY(&mSendMutex);
    MUTEX_DESTROY(&mRecvMutex);
}

//! Parse properties, instantiate a stage object
Stage* CTestStage::makeStage(const std::string& tag)
{
    CTestStage* stage = new CTestStage(tag.c_str());
    if (stage == NULL)
    {
        LOG_ERROR("new CTestStage failed");
        return NULL;
    }
    stage->setProperties();
    return stage;
}

//! Set properties for this object set in stage specific properties
bool CTestStage::setProperties()
{
    std::string stageNameStr(stageName);
    std::map<std::string, std::string> section = theGlobalProperties()->get(stageNameStr);

    std::map<std::string, std::string>::iterator it;

    std::string key;

    key = "ServerHostname";
    it = section.find(key);
    if (it != section.end())
    {
        mPeerEp.setHostName(it->second.c_str());
    }
    else
    {
        LOG_ERROR("Not set server hostname");
        return false;
    }

    key = "ServerPort";
    it = section.find(key);
    if (it != section.end())
    {
        int port;
        CLstring::strToVal(it->second, port);
        mPeerEp.setPort(port);
    }
    else
    {
        LOG_ERROR("Not set port");
        return false;
    }

    key = "TestTime";
    it = section.find(key);
    if (it != section.end())
    {
        CLstring::strToVal(it->second, mTestTimes);
    }
    else
    {
        mTestTimes = -1;
    }

    key = "TestFile";
    it = section.find(key);
    if (it != section.end())
    {
        mTestFile = it->second;
    }

    return true;
}

//! Initialize stage params and validate outputs
bool CTestStage::initialize()
{
    LOG_TRACE("Enter");

    std::list<Stage*>::iterator stgp = nextStageList.begin();
    mTimerStage = *(stgp++);
    mCommStage  = *(stgp++);

    ASSERT(dynamic_cast<TimerStage *>(mTimerStage),
            "The next stage isn't TimerStage");

    CTestStatEvent *tev = new CTestStatEvent();
    if (tev == NULL)
    {
        LOG_ERROR("No memory to alloc CTestStatEvent");
        return false;
    }

    startTimer(tev, 10);
    LOG_TRACE("Exit");
    return true;
}

//! Cleanup after disconnection
void CTestStage::cleanup()
{
    LOG_TRACE("Enter");

    LOG_TRACE("Exit");
}

void CTestStage::handleEvent(StageEvent* event)
{
    LOG_TRACE("Enter\n");

    TriggerTestEvent *tev = NULL;
    CommEvent        *cev = NULL;

    if ((tev = dynamic_cast<TriggerTestEvent *>(event) ))
    {
        return triggerTestEvent(event);
    }
    else if( (cev = dynamic_cast<CommEvent *>(event)) )
    {
        return recvRequest(event);
    }
    else
    {
        LOG_ERROR("Unknow type event");
        event->done();
    }

    LOG_TRACE("Exit\n");
    return;
}

void CTestStage::callbackEvent(StageEvent* event, CallbackContext* context)
{
    LOG_TRACE("Enter\n");

    TriggerTestEvent *tev = NULL;
    CommEvent *cev = NULL;
    CTestStatEvent *sev = NULL;

    if ( (tev = dynamic_cast<TriggerTestEvent *>(event)) )
    {
        return retriggerTestEvent(event);
    }
    else if ( (cev = dynamic_cast<CommEvent *>(event)) )
    {
        return recvResponse(event);
    }
    else if( (sev = dynamic_cast<CTestStatEvent *>(event)))
    {
        outputStat(event);
    }
    else
    {
        LOG_ERROR("Unknow type event");
        event->done();
        return ;
    }

    LOG_TRACE("Exit\n");
    return;
}

void CTestStage::sendRequest()
{
    Request *req = new Request();
    if (req == NULL)
    {
        LOG_ERROR("No memory for req");
        return ;
    }

    CommEvent *cev = new CommEvent(req, NULL);
    if (cev == NULL)
    {
        LOG_ERROR("No memory for CommEvent");
        return ;
    }

    CompletionCallback *cb = new CompletionCallback(this, NULL);
    if (cb == NULL)
    {
        LOG_ERROR("Failed to new callback");

        delete cev;

        return;
    }
    cev->pushCallback(cb);

    cev->getTargetEp() = mPeerEp;
    MsgDesc &md = cev->getRequest();

    if (mTestFile.empty() == false)
    {
        std::string confPath = mTestFile;
        u64_t fileSize = 0;
        int rc = getFileSize(confPath.c_str(), fileSize);
        if (rc)
        {
            LOG_ERROR("Failed to get file size %s", confPath.c_str());
            //req will be delete in cev
            delete cev;
            return ;
        }
        md.attachFilePath = confPath;
        md.attachFileLen  = fileSize;
        md.attachFileOffset = 0;

        char *outputData = NULL;
        size_t readSize = 0;
        rc = readFromFile(confPath, outputData, readSize);
        if (rc)
        {
            LOG_ERROR("Failed to read data of %s, rc:%d:%s",
                    confPath.c_str(), errno, strerror(errno));
            delete cev;
            return ;
        }
        IoVec::vec_t *iov = new IoVec::vec_t;
        if (iov == NULL)
        {
            LOG_ERROR("No memory to IoVec::vec_t");
            delete cev;
            free(outputData);

            return ;
        }
        iov->base = outputData;
        iov->size = (int)readSize;

        md.attachMems.push_back(iov);
    }

    mCommStage->addEvent(cev);

    MUTEX_LOCK(&mSendMutex);
    mSendCounter++;
    MUTEX_UNLOCK(&mSendMutex);

    LOG_DEBUG("Successfully issue CommEvent");
}

void CTestStage::startTimer(StageEvent *tev, int seconds)
{
    CompletionCallback *cb = new CompletionCallback(this, NULL);
    if (cb == NULL)
    {
        LOG_ERROR("Failed to new callback");

        tev->done();

        return;
    }

    TimerRegisterEvent *tmEvent = new TimerRegisterEvent(tev,
            seconds * USEC_PER_SEC);
    if (tmEvent == NULL)
    {
        LOG_ERROR("Failed to new TimerRegisterEvent");

        delete cb;

        tev->done();

        return;
    }

    tev->pushCallback(cb);
    mTimerStage->addEvent(tmEvent);
}

void CTestStage::recvRequest(StageEvent *event)
{
    MUTEX_LOCK(&mRecvMutex);
    mRecvCounter++;
    MUTEX_UNLOCK(&mRecvMutex);

    CommEvent *cev = dynamic_cast<CommEvent *>(event);

    if (cev->isfailed() == true )
    {
        LOG_ERROR("CommEvent is failed, status:%d", (int)cev->getStatus());
        cev->done();
        return ;
    }

    MsgDesc &md = cev->getRequest();

    if (md.attachFilePath.empty() == false)
    {
        LOG_INFO("Receive the file %s", md.attachFilePath.c_str());
    }
    else
    {
        LOG_DEBUG("Not receive the file");
    }

    std::vector<IoVec::vec_t*>::iterator it;
    for(it = md.attachMems.begin(); it != md.attachMems.end(); it++)
    {
        std::string path = md.attachFilePath + "_iov";
        int rc = writeToFile(path, (const char *)(*it)->base, (*it)->size, "a");
        if (rc)
        {
            LOG_ERROR("Failed to write %s", path.c_str());
        }
    }

    Response *response = new Response(CommEvent::SUCCESS, "Success");
    MsgDesc mdrsp;
    mdrsp.message = response;

    cev->setResponse(&mdrsp);

    cev->done();

    MUTEX_LOCK(&mSendMutex);
    mSendCounter++;
    MUTEX_UNLOCK(&mSendMutex);

    LOG_DEBUG("Successfully handle request");

    return ;

}

void CTestStage::recvResponse(StageEvent *event)
{
    MUTEX_LOCK(&mRecvMutex);
    mRecvCounter++;
    MUTEX_UNLOCK(&mRecvMutex);

    CommEvent *cev = dynamic_cast<CommEvent *>(event);
    if (cev->isfailed() == true )
    {
        LOG_ERROR("CommEvent is failed, status:%d", (int)cev->getStatus());

    }

    Response *response = (Response *)cev->getResponseMsg();
    if (response == NULL)
    {
        LOG_ERROR("CommEvent no response");
        cev->done();

        return ;
    }

    LOG_DEBUG("Response status:%d:%s", response->mStatus, response->mErrMsg);
    cev->done();

    return ;

}

void CTestStage::triggerTestEvent(StageEvent *event)
{
    LOG_TRACE("Handle TestEvent");

    TriggerTestEvent *tev = dynamic_cast<TriggerTestEvent *>(event);
    bool finished = false;

    if (mTestTimes == -1)
    {
        int i = 0;
        while (i < ONE_TIME_NUM)
        {
            sendRequest();
            i++;
        }

        MUTEX_LOCK(&tev->mTestMutex);
        tev->mTimes++;
        MUTEX_UNLOCK(&tev->mTestMutex);
        LOG_INFO("Finish issue %d times", tev->mTimes * ONE_TIME_NUM);

        //finished = true;

    }
    else
    {
        sendRequest();

        MUTEX_LOCK(&tev->mTestMutex);
        tev->mTimes++;
        if (tev->mTimes >= mTestTimes)
        {
            finished = true;
        }
        MUTEX_UNLOCK(&tev->mTestMutex);
        LOG_INFO("Finish issue %d times", tev->mTimes);
    }


    if (finished == true)
    {
        event->done();
        return;
    }

    startTimer(tev, tev->mSleepTime);

    return;
}

void CTestStage::retriggerTestEvent(StageEvent *event)
{

    static int i = 0;

    LOG_DEBUG("Handle Callback %d", i++);

    addEvent(event);

    LOG_DEBUG("Finish handle");
}

void CTestStage::outputStat(StageEvent *event)
{
    u32_t sendTPS = 0;
    u32_t recvTPS = 0;

    MUTEX_LOCK(&mSendMutex);
    sendTPS = mSendCounter - mLastSendCounter;
    mLastSendCounter = mSendCounter;
    MUTEX_UNLOCK(&mSendMutex);

    MUTEX_LOCK(&mRecvMutex);
    recvTPS = mRecvCounter - mLastRecvCounter;
    mLastRecvCounter = mRecvCounter;
    MUTEX_UNLOCK(&mRecvMutex);

    LOG_INFO("Send TPS:%u, Recv TPS:%u, SendTotal:%u, RecvTotal:%u",
            sendTPS/10, recvTPS/10, mLastSendCounter, mLastRecvCounter);

    startTimer(event, 10);
}
