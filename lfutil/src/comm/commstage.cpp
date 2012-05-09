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
 * commstage.cpp
 *
 *  Created on: Apr 25, 2012
 *      Author: Longda Feng
 */

#include "conf/ini.h"
#include "lang/lstring.h"
#include "seda/timerstage.h"

#include "comm/commstage.h"
#include "net/netserver.h"
#include "net/iovutil.h"

//! Constructor
CommStage::CommStage(const char* tag) :
        Stage(tag), mServer(false), mNet(NULL), mNextStage(NULL)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    MUTEX_INIT(&mCounterMutex, &attr);
}

//! Destructor
CommStage::~CommStage()
{
    MUTEX_DESTROY(&mCounterMutex);
}

//! Parse properties, instantiate a stage object
Stage* CommStage::makeStage(const std::string& tag)
{
    CommStage* stage = new CommStage(tag.c_str());
    if (stage == NULL)
    {
        LOG_ERROR("new CommStage failed");
        return NULL;
    }

    bool ret = stage->setProperties();
    if (ret == false)
    {
        LOG_ERROR("Failed to set %s properties", tag.c_str());
        delete stage;
        return NULL;
    }
    return stage;
}

//! Set properties for this object set in stage specific properties
bool CommStage::setProperties()
{
    std::string stageNameStr(stageName);
    std::map<std::string, std::string> section = theGlobalProperties()->get(
            stageNameStr);

    std::map<std::string, std::string>::iterator it;

    std::string key;

    key = "server";
    it = section.find(key);
    if (it != section.end())
    {
        CLstring::strToVal(it->second, mServer);
        LOG_INFO("Current component is one server");
    }
    else
    {
        LOG_INFO("Current component isn't one server");
    }

    if (Conn::setLocalEp(section, mServer) == false)
    {
        return false;
    }

    Conn::setSocketProperty(section);

    return true;
}

//! Initialize stage params and validate outputs
bool CommStage::initialize()
{
    LOG_TRACE("Enter");

    ASSERT(nextStageList.size() == 1,
            "CommStage next stage should be only one");

    std::list<Stage*>::iterator stgp = nextStageList.begin();
    mNextStage = *(stgp++);

    if (mServer == true)
    {
        int port = Conn::getLocalEp().getPort();
        mNet = new NetServer(this, (unsigned short) port);
    }
    else
    {
        mNet = new Net(this);
    }
    if (!mNet)
    {
        LOG_ERROR( "Failed to alloc new net");
        return false;
    }

    int rc = mNet->setup();
    if (rc)
    {
        LOG_ERROR("Failed to setup network, %d", rc);
        return false;
    }

    LOG_TRACE("Exit");
    return true;
}

//! Cleanup after disconnection
void CommStage::cleanup()
{
    LOG_TRACE("Enter");

    mNet->shutdown();
    delete mNet;

    mNet = NULL;
    LOG_INFO("Successfully shutdown net");

    LOG_TRACE("Exit");
}

void CommStage::handleEvent(StageEvent* event)
{
    LOG_TRACE("Enter\n");

    CommEvent *cev = NULL;
    CommSendEvent *sev = NULL;
    CommRecvEvent *rev = NULL;

    if ((cev = dynamic_cast<CommEvent*>(event)))
    {
        sendRequest(cev);
        LOG_TRACE("Exit\n");
        return;
    }
    else if ((sev = dynamic_cast<CommSendEvent *>(event)))
    {
        sendData(sev);
        LOG_TRACE("Exit");
        return;
    }
    else if ((rev = dynamic_cast<CommRecvEvent *>(event)))
    {
        recvData(rev);
        LOG_TRACE("Exit");
        return;
    }
    else
    {
        LOG_ERROR("Unknow type event");
        event->done();
        return;
    }

    LOG_TRACE("Exit\n");
    return;
}

void CommStage::callbackEvent(StageEvent* event, CallbackContext* context)
{
    LOG_TRACE("Enter\n");
    CommEvent *cev = NULL;
    if ((cev = dynamic_cast<CommEvent*>(event)))
    {
        sendResponse(cev);
        LOG_TRACE("Exit\n");
        return;
    }
    else
    {
        LOG_ERROR("Unknow type event");
        event->done();
        return;
    }

    LOG_TRACE("Exit\n");
    return;
}

void CommStage::cleanupFailedResp(MsgDesc &md, CommEvent* cev, Conn *conn,
        CommEvent::status_t errCode)
{
    cev->completeEvent(errCode);

    conn->release();
}

void CommStage::cleanupFailedReq(MsgDesc &md, CommEvent* cev, Conn *conn,
        CommEvent::status_t errCode)
{
    conn->removeEventEntry(cev->getRequestId());

    cev->completeEvent(errCode);

    conn->release();
}

void CommStage::sendRequest(CommEvent *cev)
{
    LOG_TRACE("enter\n");

    // Set event's status to pending
    cev->setStatus(CommEvent::PENDING);

    MsgDesc md = cev->getRequest();
    Message* reqMsg = md.message;
    if (reqMsg == NULL || dynamic_cast<Request *>(reqMsg) == NULL)
    {
        cev->completeEvent(CommEvent::INVALID_PARAMETER);
        return;
    }

    Request& req = *(Request *) reqMsg;

    // Get a connection to the target end point
    EndPoint& tep = cev->getTargetEp();

    Conn *conn = NULL;
    try
    {
        conn = mNet->getConn(tep);
    } catch (NetEx ex)
    {
        std::string errMsg = "can't open conn:" + ex.message;
        LOG_ERROR( errMsg.c_str());

        cev->completeEvent(CommEvent::CONN_FAILURE);
        return;
    }

    req.mSourceEp = Conn::getLocalEp();
    req.mVersion = VERSION_NUM;

    // For now, msgCounter is shared among all connections. It might be
    // better if a separate counter is kept for each connection.

    // Enter the event in a map so we can match it when the response arrives
    // We use the map's mutex for protecting the counter too.
    MUTEX_LOCK(&mCounterMutex);
    req.mId = mMsgCounter;
    mMsgCounter++;
    MUTEX_UNLOCK(&mCounterMutex);
    cev->setRequestId(req.mId);

    conn->addEventEntry(req.mId, cev);

    IoVec** iovs = new IoVec*[md.attachMems.size() + 1];
    if (iovs == NULL)
    {
        LOG_ERROR("Failed to create rpc IoVec list");
        cleanupFailedReq(md, cev, conn, CommEvent::RESOURCE_FAILURE);
        delete[] iovs;

        return;
    }

    if (prepareReqIovecs(md, iovs, cev, conn, this))
    {
        LOG_ERROR("Failed to create rpc IoVec buffer");
        cleanupFailedReq(md, cev, conn, CommEvent::RESOURCE_FAILURE);
        delete[] iovs;
        return;
    }

    Conn::status_t rc = conn->send(md.attachMems.size() + 1, iovs);
    if (rc != Conn::SUCCESS)
    {
        cleanupFailedReq(md, cev, conn, CommEvent::SEND_FAILURE);
        // the iovs buffer have been put into Conn::mSendQ
        // whatever success or not, Conn will free the iovs buffer
        delete[] iovs;
        return;
    }

    // send success
    conn->messageOut();

    conn->release();
    delete[] iovs;

    LOG_TRACE("exit\n");
}

// Send response back to client
void CommStage::sendResponse(CommEvent* cev)
{
    LOG_TRACE("enter\n");

    MsgDesc md = cev->getResponse();
    Message* respMsg = md.message;
    ASSERT(respMsg, "bad response message");

    // Get a connection to the client EndPoint
    EndPoint& cep = cev->getTargetEp();

    /**
     * @@@ FIXME
     */
//    MUTEX_LOCK(&mCounterMutex);
//
//    resp.mId = (mMsgCounter);
//    mMsgCounter++;
//    MUTEX_UNLOCK(&mCounterMutex);
    Conn *conn = 0;
    try
    {
        conn = mNet->getConn(cep, true);
    } catch (NetEx ex)
    {
        LOG_ERROR("can't get conn to %s:%d: reason: %s\n",
                cep.getHostName(), cep.getPort(), ex.message.c_str());
        cev->completeEvent(CommEvent::CONN_FAILURE);
        return;
    }

    // Prepare response message and attachments
    IoVec** iovs = new IoVec*[md.attachMems.size() + 1];
    if (iovs == NULL)
    {
        LOG_ERROR("Failed to create rpc IoVec list");
        cleanupFailedResp(md, cev, conn, CommEvent::RESOURCE_FAILURE);
        delete[] iovs;

        return;
    }

    if (prepareRespIovecs(md, iovs, cev, this))
    {
        LOG_ERROR("Failed to create rpc IoVec buffer");
        cleanupFailedResp(md, cev, conn, CommEvent::RESOURCE_FAILURE);
        delete[] iovs;
        return;
    }

    Conn::status_t st = conn->send(md.attachMems.size() + 1, iovs);
    if (st != Conn::SUCCESS)
    {
        if (st == Conn::CONN_ERR_BROKEN)
        {
            LOG_ERROR("failed in sending response: connection broken\n");
            // don't cleanup the CommEvent here
            // since it has been deleted in conn->send()
            // If conn->send() returns with CONN_ERR_BROKEN, the send operation
            // has failed because the socket was broken, which would have
            // triggered the iovec's cleanup for the connection. This in turn
            // will complete all CommEvent's for which iovec's are posted.
            // See comment above the invocation to conn->send()
        }
        else
        {
            // Depending on the type of failure, the incoming event may
            // need to be completed here with
            // completeEvent(cev, CommEvent::SEND_FAILURE);
            LOG_ERROR("unknown send failure reason\n");
        }
    }
    delete[] iovs;
    conn->messageOut();
    conn->release();

    LOG_TRACE("exit\n");
}

void CommStage::sendData(CommSendEvent *event)
{
    Conn *conn = (Conn*) (event->mConn);

    conn->sendProgress(true); // true indicates that socket is ready
    //conn has already been acquire in Net::SendThread
    conn->release();

    event->done();

    return;
}

void CommStage::recvData(CommRecvEvent *event)
{
    // Read from socket
    ConnMgr* cm = &mNet->getConnMgr();
    Conn *conn = NULL;
    int sock = event->mSocket;

    cm->lock();
    conn = cm->find(sock);
    cm->unlock();
    if (NULL == conn)
    {
        LOG_ERROR("No connection with socket %d", sock);
        conn->release();
        event->done();
        return;
    }

    Conn::status_t crc = conn->recvProgress(true);

    if (crc == Conn::SUCCESS)
    {
        conn->release();
        event->done();
        return;
    }
    else if (crc == Conn::CONN_READY)
    {
        // There is more data on this connection.

        // old logic is add the socket/connection to recv thread's readyConns

        //here just add it to epoll

        conn->release();

        mNet->addToRecvSelector(sock);

        event->done();
        return;
    }
    else if (crc == Conn::CONN_ERR_BROKEN)
    {
        LOG_DEBUG("removing broken conn");

        cm->lock();
        mNet->removeConn(sock);
        cm->unlock();
        conn->release();

        event->done();
        return;
    }
    else
    {
        // other error
        // CONN_ERR_UNAVAIL
        conn->release();
        LOG_ERROR("recvProgress error: %d", crc);

        event->done();
        return;
    }
}

void CommStage::setDeserializable(Deserializable *deserializer)
{
    Conn::setDeserializable(deserializer);
}

void CommStage::setSelectDir(CSelectDir *selectDir)
{
    Conn::setSelectDir(selectDir);
}

Stage *CommStage::getNextStage()
{
    return mNextStage;
}
