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
 * conncb.cpp
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */

#include "io/io.h"
#include "time/datetime.h"
#include "net/conn.h"
#include "net/iovutil.h"
#include "net/netex.h"
#include "comm/commstage.h"



int Conn::connCallback(Conn::conn_t state)
{
    LOG_TRACE("enter\n");

    int rc = 0;

    CommStage *cs = (CommStage *)gCommStage;

    if (state == Conn::ON_CONNECT)
    {
        LOG_TRACE("on connect");

        IoVec* iov = prepareRecvHeader(this, cs);
        if (iov == NULL)
        {
            LOG_ERROR("Failed to prepare receive header");
            return CONN_ERR_NOMEM;

        }
        // Prepost the IoVec for the first incoming message
        Conn::status_t crc = postRecv(iov);
        if (crc != Conn::SUCCESS)
        {
            LOG_ERROR("failed to post IoVec %d\n", (int)crc);
            iov->cleanup();
            delete iov;
            return crc;
        }

        // won't free mRecvCb only when disconnect connection
        mRecvCb = (cb_param_t *)iov->getCallbackParam();

        LOG_TRACE("exit\n");

        return SUCCESS;
    }
    else if (state == Conn::ON_DISCONNECT)
    {
        LOG_TRACE("on disconnect");

        MUTEX_LOCK(&mEventMapMutex);
        for (std::map<u32_t, CommEvent*>::iterator ei = mSendEventMap.begin();
                ei != mSendEventMap.end(); ei++)
        {
            CommEvent *cev = (ei->second);
            cev->completeEvent(CommEvent::CONN_FAILURE);
        }
        mSendEventMap.clear();
        MUTEX_UNLOCK(&mEventMapMutex);


        // release the cbp associated with this connection
        if (mRecvCb)
        {
            delete mRecvCb;
            mRecvCb = NULL;
        }

        ((CommStage *)gCommStage)->clearSelector(mSock);

        LOG_TRACE("exit\n");
        return SUCCESS;
    }
    else
    {
        LOG_ERROR( "%s", "unknown conn callback state");
    }
    LOG_TRACE("exit\n");

    return rc;
}

int Conn::recvErrCb(IoVec *iov, cb_param_t* cbp, IoVec::state_t state, bool freeIov)
{
    LOG_TRACE("enter");
    if (freeIov)
    {
        if (iov->getAllocType() == IoVec::SYS_ALLOC)
        {
            delete (char *)iov->getBase();
            iov->setBase(NULL);
        }

        delete iov;
    }

    CommEvent* cev = cbp->cev;
    cbp->cev = NULL;

    if (cev && !cbp->eventDone)
    {
        if (cev->isServerGen())
        {
            /**
             * @@@ FIXME this maybe trigger issues
             * when other stage is handling the event
             *
             * Cleanup server side events with attachments when the
             * client breaks the connection before the full attachment
             * is received
             */
            //
            cbp->eventDone = true;
            cev->done();
        }
        else
        {
            // Cleanup the event on the client side when the server
            // breaks the connection
            CommStage* cs = (CommStage *)cbp->cs;
            ASSERT(cs, "bad CommStage pointer");

            Conn *conn = cbp->conn;
            conn->removeEventEntry(cbp->reqId);

            if (cbp->eventDone == false)
            {
                cev->completeEvent(CommEvent::CONN_FAILURE);
                cbp->eventDone = true;
            }
        }
    }

    LOG_TRACE("error exit");
    return SUCCESS;
}

IoVec** Conn::allocIoVecs(const size_t baseLen, IoVec *iov, int &blockNum)
{
    int blockCount = (baseLen + gMaxBlockSize - 1) / gMaxBlockSize;
    IoVec **iovs = new IoVec *[blockCount];
    if (iovs == NULL)
    {
        LOG_ERROR("Failed to alloc iovs");
        return NULL;
    }

    bool success = true;
    int i = 0;
    for (; i < blockCount - 1; i++)
    {
        char *base = new char[gMaxBlockSize];
        if (base == NULL)
        {
            success = false;
            break;
        }
        iovs[i] = new IoVec(base, gMaxBlockSize, Conn::recvCallback, NULL);
        if (iovs[i] == NULL)
        {
            delete base;
            success = false;
            break;
        }
    }

    char *base = NULL;
    if (success == false)
    {
        goto cleanupIovs;
    }

    // set  the last one
    base = new char[baseLen % gMaxBlockSize];
    if (base == NULL)
    {
        goto cleanupIovs;
    }

    if (iov)
    {
        iov->reset();
        iov->setBase(base);
        iov->setSize(baseLen);

        // the last one maybe exist callback/cbp
    }
    else
    {
        iov = new IoVec(base, baseLen % gMaxBlockSize, Conn::recvCallback, NULL);
    }


    iovs[i] = iov;
    blockNum = blockCount;
    return iovs;

cleanupIovs:
    for (int j = 0; j < i; j++)
    {
        iovs[j]->cleanup();
        delete iovs[j];
    }
    delete iovs;
    return NULL;
}

int Conn::repostIoVecs(Conn* conn, IoVec* iov, const size_t baseLen)
{
    char *base = static_cast<char *>(iov->getBase());
    if(base && iov->getAllocType() == IoVec::SYS_ALLOC)
        delete [] base;

    int blockNum = 0;
    IoVec** iovs = allocIoVecs(baseLen, iov, blockNum);
    if (iovs == NULL)
    {
        return Conn::CONN_ERR_NOMEM;
    }

    conn->postRecv(blockNum, iovs);
    delete[] iovs;

    return Conn::SUCCESS;
}

int Conn::repostIoVec(Conn* conn, IoVec* iov, size_t baseLen)
{
    char *base = static_cast<char *>(iov->getBase());
    if(base && iov->getAllocType() == IoVec::SYS_ALLOC)
        delete [] base;

    // set  the last one
    base = new char[baseLen];
    if (base == NULL)
    {
        LOG_ERROR("Failed to alloc %llu memory ", (u64_t)baseLen);
        return Conn::CONN_ERR_NOMEM;
    }

    iov->reset();
    iov->setBase(base);
    iov->setSize(baseLen);

    conn->postRecv(iov);

    return Conn::SUCCESS;
}

int Conn::repostReusedIoVec(const size_t baseLen, IoVec *iov, cb_param_t* cbp)
{
    int blockCount = (baseLen + gMaxBlockSize - 1) / gMaxBlockSize;

    u32_t blockSize = gMaxBlockSize;
    if (blockSize > baseLen )
    {
        blockSize = baseLen;
    }

    char *base = static_cast<char *>(iov->getBase());
    if (base && iov->getAllocType() == IoVec::SYS_ALLOC)
        delete[] base;

    // set  the last one
    base = new char[blockSize];
    if (base == NULL)
    {
        LOG_ERROR("Failed to alloc %llu memory ", (u64_t)baseLen);
        return Conn::CONN_ERR_NOMEM;
    }

    iov->reset();
    iov->setBase(base);
    iov->setSize(blockSize);

    cbp->conn->postRecv(iov);

    cbp->remainVecs = blockCount;

    return SUCCESS;
}

int Conn::recvHeaderCb(IoVec *iov, cb_param_t* cbp, IoVec::state_t state)
{
    LOG_TRACE("enter");

    LOG_TRACE(MSG_TYPE_RPC);

    PackHeader *hdr = static_cast<PackHeader *>(iov->getBase());

    ASSERT((iov->getSize() == HDR_LEN), "bad IoVec size");

    u64_t msgLen, attLen, fileLen;
    CLstring::strToVal(std::string(hdr->mMsgLen), msgLen);
    CLstring::strToVal(std::string(hdr->mAttLen), attLen);
    CLstring::strToVal(std::string(hdr->mFileLen), fileLen);

    // Keep cbp as the callback argument. Should not free it.
    // attLen could be 0 for message with no attachment
    Conn *conn  = cbp->conn;
    cbp->reset();

    cbp->conn = conn;
    cbp->cs   = gCommStage;
    cbp->attLen = attLen;
    cbp->fileLen = fileLen;

    if (strncmp(hdr->mType, MSG_TYPE_RPC, sizeof(MSG_TYPE_RPC)) != 0)
    {
        LOG_ERROR("Don't support %s", hdr->mType);

        u64_t leftSize = msgLen + cbp->attLen + cbp->fileLen;

        prepare2Drain(iov, cbp, conn, leftSize);

        LOG_TRACE("exit");

        return Conn::CONN_ERR_MISMATCH;
    }

    ASSERT((iov->getAllocType() == IoVec::SYS_ALLOC), "bad alloc");

    //set next stage
    conn->setNextRecv(Conn::MESSAGE);

    int rc = repostIoVec(conn, iov, msgLen);
    if (rc)
    {
        LOG_ERROR("Failed to alloc memory");

        u64_t leftSize = msgLen + cbp->attLen + cbp->fileLen;

        prepare2Drain(iov, cbp, conn, leftSize);
        LOG_TRACE("exit");
        return rc;
    }
    LOG_TRACE("exit");
    return SUCCESS;
}

void Conn::prepare2Drain(IoVec *iov, cb_param_t* cbp, Conn *conn, u64_t leftSize)
{
    LOG_TRACE("enter");

    int   rc = SUCCESS;
    if (leftSize > 0)
    {
        cbp->drainLen = leftSize;
        rc = repostReusedIoVec(leftSize, iov, cbp);
        if (rc)
        {
            throw NetEx(Conn::CONN_ERR_NOMEM, "No memory to drain");
        }
        conn->setNextRecv(Conn::DRAIN);
    }
    else
    {
        recvErrCb(iov, cbp, IoVec::ERROR, false);
        cbp->drainLen = 0;
        rc = repostIoVec(conn, iov, HDR_LEN);
        if (rc)
        {
            throw NetEx(Conn::CONN_ERR_NOMEM, "No memory to drain");
        }
        conn->setNextRecv(Conn::HEADER);
    }

    LOG_TRACE("exit");
    return ;
}

int Conn::pushAttachMessage(Conn *conn, MsgDesc &md, cb_param_t* cbp)
{
    IoVec** iovs = new IoVec*[md.attachMems.size()];
    if (iovs == NULL)
    {
        LOG_ERROR("Failed to alloc IoVec** ");

        return Conn::CONN_ERR_NOMEM;
    }

    bool success = true;
    int i;
    for (i = 0; i < (int)md.attachMems.size(); i++)
    {
        iovs[i] = new IoVec(md.attachMems[i], IoVec::USER_ALLOC);
        if (iovs[i] == NULL)
        {
            success = false;
            break;
        }
        iovs[i]->setCallback(recvCallback, cbp);
    }

    if (success == false)
    {
        for (int j = 0; j < i; j++)
        {
            delete iovs[j];
        }

        delete[] iovs;
        return Conn::CONN_ERR_NOMEM;
    }

    // the last one need set callback
    cbp->remainVecs = md.attachMems.size();
    conn->postRecv(md.attachMems.size(), iovs);
    delete[] iovs;

    return SUCCESS;
}

void Conn::cleanMdAttach(MsgDesc &md)
{
    for (int i = 0; i < (int)md.attachMems.size(); i++)
    {
        delete (char *)md.attachMems[i]->base;
        delete md.attachMems[i];
    }
    md.attachMems.clear();
}

int Conn::allocAttachIoVecs(MsgDesc &md, const size_t baseLen)
{
    int blockCount = (baseLen + gMaxBlockSize - 1) / gMaxBlockSize;

    for (int i = 0; i < blockCount - 1; i++)
    {
        char *base = new char[gMaxBlockSize];
        if (base == NULL)
        {
            LOG_ERROR("No memory for IoVec::vec_t->base %d", (gMaxBlockSize));
            cleanMdAttach(md);
            return Conn::CONN_ERR_NOMEM;
        }
        IoVec::vec_t *iov = new IoVec::vec_t();
        if (iov == NULL)
        {
            LOG_ERROR("No memory for IoVec::vec_t");

            delete base;

            cleanMdAttach(md);
            return Conn::CONN_ERR_NOMEM;
        }
        iov->base = base;
        iov->size =  gMaxBlockSize;

        md.attachMems.push_back(iov);
    }

    // set  the last one
    int lastSize = baseLen - ((blockCount - 1) * gMaxBlockSize);
    char *base = new char[lastSize];
    if (base == NULL)
    {
        LOG_ERROR("No memory for IoVec::vec_t->base %d", (baseLen % gMaxBlockSize));

        cleanMdAttach(md);
        return Conn::CONN_ERR_NOMEM;
    }

    IoVec::vec_t *iov = new IoVec::vec_t();
    if (iov == NULL)
    {
        LOG_ERROR("No memory for IoVec::vec_t");
        delete base;

        cleanMdAttach(md);
        return Conn::CONN_ERR_NOMEM;
    }
    iov->base = base;
    iov->size = lastSize;

    md.attachMems.push_back(iov);

    return 0;
}

int Conn::recvPrepareFileIov(cb_param_t* cbp, Conn *conn)
{
    u32_t blockSize = gMaxBlockSize;

    cbp->fileOffset = 0;
    if (cbp->fileLen <= blockSize)
    {
        blockSize = cbp->fileLen;
    }

    char *base = new char[blockSize];
    if (base == NULL)
    {
        LOG_ERROR("No memory for file download buffer %u", blockSize);

        return Conn::CONN_ERR_NOMEM;
    }

    IoVec * iov = new IoVec(base, blockSize, recvCallback, cbp);
    if (iov == NULL)
    {
        LOG_ERROR("No memory for iovec");
        return Conn::CONN_ERR_NOMEM;
    }
    cbp->remainVecs = (cbp->fileLen + gMaxBlockSize - 1)/gMaxBlockSize;

    conn->setNextRecv(Conn::ATTACHFILE);
    conn->postRecv(iov);

    return SUCCESS;
}

int Conn::recvReqMsg(Request *msg, IoVec *iov, cb_param_t* cbp, Conn *conn)
{
    LOG_TRACE("enter");

    // This is a request message
    LOG_DEBUG("received a request message");

    bool eventReady = false;

    MsgDesc md;

    md.message = msg;

    if (cbp->attLen > 0)
    {

        int rc = allocAttachIoVecs(md, cbp->attLen);
        if (rc)
        {
            LOG_ERROR("No memory to receive attachment, size :%u", cbp->attLen);
            return rc;
        }

        rc = pushAttachMessage(conn, md, cbp);
        if (rc)
        {
            LOG_ERROR("Failed to push iovs when receive reqest");
            cleanMdAttach(md);

            return rc;
        }

        conn->setNextRecv(Conn::ATTACHMENT);
        eventReady = false;

    } else if (cbp->fileLen)
    {
        int rc = recvPrepareFileIov(cbp, conn);
        if (rc)
        {
            LOG_ERROR("Failed to prepare file iov");
            return rc;
        }
        eventReady = false;
    }
    else
    {
        eventReady = true;
    }

    ASSERT((cbp->cev == 0), "cev must be 0");

    CommEvent *cev = new CommEvent(&md);
    cev->setSock(conn->getSocket());
    cev->setServerGen();
    // Record the id of the incoming request
    cev->setRequestId(msg->mId);
    cev->getTargetEp() = conn->getPeerEp();

    cbp->cev = cev;

    checkEventReady(eventReady, conn, iov, cbp);

    LOG_TRACE("exit");
    return SUCCESS;
}

int Conn::recvPrepareRspAttach(MsgDesc &mdresp, cb_param_t* cbp, Conn *conn)
{
    LOG_TRACE("enter");

    bool alloc = true;
    int rc = 0;
    if (mdresp.attachMems.size() == 0)
    {
        // user don't provide the memory
        rc = allocAttachIoVecs( mdresp, cbp->attLen);
        if (rc)
        {
            LOG_ERROR("No memory to receive attachment, size :%u", cbp->attLen);
            return rc;
        }

    }
    else if (mdresp.attachMems.size() > 0)
    {
        alloc = false;

        u32_t allocLen = 0;
        for (int i = 0; i < (int)mdresp.attachMems.size(); i++)
        {
            allocLen += mdresp.attachMems[i]->size;
        }

        if (allocLen == cbp->attLen)
        {
            //everything is perfect
        }
        else if (allocLen > cbp->attLen)
        {
            //truncate some buffer size
            int leftSize = cbp->attLen;

            int i = 0;
            for (i = 0; leftSize > 0 ; i++ )
            {
                if (mdresp.attachMems[i]->size > leftSize)
                {
                    mdresp.attachMems[i]->size = leftSize;
                    leftSize  = 0;
                    i++;
                    break;
                }
                else
                {
                    leftSize -= mdresp.attachMems[i]->size;
                }
            }

            for (;i < (int)mdresp.attachMems.size(); i++)
            {
                mdresp.attachMems[i]->size = 0;
            }

        }
        else
        {
            // allocLen < cbp->Len
            rc = allocAttachIoVecs(mdresp, cbp->attLen - allocLen );
            if (rc)
            {
                LOG_ERROR("No memory to receive attachment, size :%u",
                        cbp->attLen);
                return rc;
            }
        }
    }// mdresp.attachMems.size() > 0

    rc = pushAttachMessage(conn, mdresp, cbp);
    if (rc)
    {
        LOG_ERROR("Failed to push attach buffer to recvQ");
        /**
         * @@@ FIXME repostIoVec exist problem
         */
        if (alloc == true)
        {
            cleanMdAttach (mdresp);
        }

        return rc;
    }

    conn->setNextRecv(Conn::ATTACHMENT);

    LOG_TRACE("exit");
    return SUCCESS;
}

void Conn::checkEventReady(bool eventReady, Conn *conn, IoVec *iov, cb_param_t *cbp)
{
    LOG_TRACE("enter");

    if (eventReady == true)
    {
        eventDone(cbp->cev, conn);

        // No attachment; get next header
        // Release the current iov->base, which held the XML and allocate
        // a new one for the new header
        conn->setNextRecv(Conn::HEADER);

        // Repost a header for a next request
        cbp->cev = 0; // reset the event

        repostIoVec(conn, iov, HDR_LEN);
    }
    else
    {
        // free the iov
        if (iov->getAllocType() == IoVec::SYS_ALLOC)
        {
            delete (char *)iov->getBase();
        }
        delete iov;
    }

    LOG_TRACE("exit");
    return ;
}

int Conn::recvRspMsg(Response *rsp, IoVec *iov, cb_param_t* cbp, Conn *conn)
{
    LOG_TRACE("enter");

    // This is a response message
    // We need to check if we have a pre-posted response for scatter
    LOG_TRACE("received a response message");

    CommEvent *cev = NULL;
    bool eventReady = false;

    cev = conn->getAndRmEvent(rsp->mId);
    if (cev == NULL)
    {
        LOG_ERROR("Failed to find CommEvent of %u in Conn::mSendMap", rsp->mId);
        return Conn::CONN_ERR_NOCEV;
    }
    ASSERT((cbp->cev == 0), "%s", "cev must be 0 here");

    cbp->cev = cev;
    // For Response message, save request id in case the connection
    // breaks or receive error occurs, which will trigger cleanupEvent()
    cbp->reqId = rsp->mId;

    /**
     * @@@ FIXME, in the old code ,here will set status
     */
    // Received response before the last io vector's callback is
    // called. Need to set event status here as the event is
    // removed from event map, and send callback cannot set event
    // status since it cannot not find it in the map
    if (cev->getStatus() == CommEvent::PENDING)
        cev->setStatus(CommEvent::SUCCESS);


    if (rsp->mStatus == CommEvent::VERSION_MISMATCH ||
        rsp->mStatus == CommEvent::MALFORMED_MESSAGE)
    {
        /**
         * @@@ TODO, when
         */
        // Found the request and this is a version mismatch or
        // malformed message error response, overwrite protocol and
        // source/target endpoints in response as they are not set
        // at server side
//        Message* reqMsg = cev->getRequestMsg();
//        if (reqMsg)
//        {
//            msg->protocol = reqMsg->protocol;
//            msg->source = reqMsg->target;
//            msg->target = reqMsg->source;
//        }
        cev->setStatus((CommEvent::status_t)rsp->mStatus);
    }


    // Check if the event has a pre-posted response
    // Whether the incoming response has or does not have attachments
    // we need to record the vectors posted by the user so we can
    // return them back to the user
    MsgDesc &mdresp = cev->getResponse();
    if (mdresp.message)
    {
        delete mdresp.message; // This will be replaced
    }
    mdresp.message = rsp;

    // Check if there are attachments
    if (cbp->attLen > 0)
    {
        int rc = recvPrepareRspAttach(mdresp, cbp, conn);
        if (rc)
        {
            LOG_ERROR("Failed to prepare buffer's of rsp attachment");
            return rc;
        }
        eventReady = false;
    }
    else if(cbp->fileLen > 0)
    {
        int rc = recvPrepareFileIov(cbp, conn);
        if (rc)
        {
            LOG_ERROR("Failed to prepare file iov");
            return rc;
        }
        eventReady = false;
    }
    else
    {
        eventReady = true;
    }


    checkEventReady(eventReady, conn, iov, cbp);

    LOG_TRACE("exit");
    return SUCCESS;
}

void Conn::eventDone(CommEvent *cev, Conn *conn)
{
    LOG_TRACE("enter");

    ASSERT(cev, "invalid comm event pointer");

    // Increment message mMsgCounter for received messages
    conn->messageIn();

    if (cev->isServerGen())
    {


        CompletionCallback* cb = new CompletionCallback(gCommStage, NULL);

        cev->pushCallback(cb);

        ((CommStage *)gCommStage)->getNextStage()->addEvent(cev);
    }
    else
    {
        /**
         * @@@ FIXME refine later
         */
        // It is very unlikely, but still possible that we can get here
        // before the send callback on the last outgoing iovec is
        // done. This callback holds a reference to the event. We use
        // busy wait on the event status to make sure that the send
        // callback is done with the event before we complete it
        while (cev->getStatus() == CommEvent::PENDING)
            usleep(100);

        // This is a response at the client. Complete event
        Response * rsp = (Response *) cev->getResponseMsg();
        cev->completeEvent((CommEvent::status_t)rsp->mStatus);

    }

    LOG_TRACE("exit");
    return ;
}

void Conn::sendBadMsgErr(Conn* conn, char * base, CommEvent::status_t errCode, const char *errMsg)
{
    LOG_TRACE("enter");

    // in order to get reqId
    Message *req = new Message(MESSAGE_BASIC);
    int rc = req->deserialize(base, req->getSerialSize());
    if (rc)
    {
        LOG_ERROR("Failed to get reqId from bad msg");
        return;
    }

    Response *rsp = new Response(errCode, errMsg);
    if (rsp == NULL)
    {
        LOG_ERROR("No memory for Response");
        return ;
    }
    MsgDesc   md(rsp);
    CommEvent *rspCev = new CommEvent(NULL, &md);
    if (rspCev == NULL)
    {
        LOG_ERROR("No memory for CommEvent");
        delete rsp;
        return ;
    }

    rspCev->setRequestId(req->mId);
    delete req;
    req = NULL;

    rspCev->setStatus(errCode);
    rspCev->setServerGen();
    rspCev->setSock(conn->getSocket());
    rspCev->getTargetEp() = conn->getPeerEp();


    ((CommStage *)gCommStage)->sendResponse(rspCev);

    LOG_TRACE("exit");
    return ;
}

int Conn::recvAttach(IoVec *iov, cb_param_t* cbp, Conn *conn)
{
    LOG_TRACE("enter");

    bool eventReady = false;

    if (cbp->fileLen == 0)
    {
        eventReady = true;
    }
    else
    {
        int rc = recvPrepareFileIov(cbp, conn);
        if (rc)
        {
            LOG_ERROR("Failed to prepare file iov");
            return rc;
        }
        eventReady = false;
    }

    checkEventReady(eventReady, conn, iov, cbp);

    LOG_TRACE("exit");
    return SUCCESS;
}

void Conn::generateRcvFileName(std::string &fileName, CommEvent *cev)
{

    std::string fileDir = gSelectDir->select();

    const char  *peerHost =  cev->getTargetEp().getHostName();

    s64_t        timestamp = Now::usec();
    std::string  ts;
    CLstring::valToStr(timestamp, ts);

    u64_t        reqId = cev->getRequestId();
    std::string  reqStr;
    CLstring::valToStr(reqId, reqStr);

    fileName = fileDir + (FILE_PATH_SPLIT) + peerHost + "_" + ts + "_" + reqStr;

    return ;
}

int Conn::recvFile(IoVec *iov, cb_param_t* cbp, Conn *conn)
{
    LOG_TRACE("enter");

    u32_t blockCount = (cbp->fileLen + gMaxBlockSize - 1)/gMaxBlockSize;
    if (cbp->remainVecs == blockCount)
    {
        // this is the first time to receive the file data

        CommEvent *cev = cbp->cev;

        std::string fileName;
        generateRcvFileName(fileName, cev);

        MsgDesc    *md = NULL;
        if (cev->isServerGen())
        {
            md = &cev->getRequest();
        }
        else
        {
            md = &cev->getResponse();
        }

        md->attachFilePath = fileName;

        int rc = writeToFile(fileName, (char *)iov->getBase(), (u32_t)iov->getSize(), "w+");
        if (rc)
        {
            LOG_ERROR("Failed to write data to file");
            return CONN_ERR_WRITEFILE;
        }
        strncpy(cbp->filePath, fileName.c_str(), sizeof(cbp->filePath) - 1);
    }
    else
    {
        std::string fileName = cbp->filePath;
        int rc = writeToFile(fileName, (char *)iov->getBase(), (u32_t)iov->getSize(), "a");
        if (rc)
        {
            LOG_ERROR("Failed to write data to file");
            return CONN_ERR_WRITEFILE;
        }
    }

    cbp->remainVecs--;

    if (cbp->remainVecs == 0)
    {
        checkEventReady(true, conn, iov, cbp);
    }

    LOG_TRACE("exit");
    return SUCCESS;

}

int Conn::recvDrain(IoVec *iov, cb_param_t* cbp, Conn *conn)
{
    LOG_TRACE("enter");

    cbp->remainVecs--;
    if (cbp->remainVecs == 0)
    {
        recvErrCb(iov, cbp, IoVec::ERROR, false);
        conn->setNextRecv(Conn::HEADER);
        repostIoVec(conn, iov, HDR_LEN);
    }
    else if (cbp->remainVecs == 1)
    {
        // the last block
        u32_t lastBlockSize = (cbp->drainLen % gMaxBlockSize);

        iov->reset();
        iov->setSize(lastBlockSize);

        conn->postRecv(iov);
    }
    else
    {
        // reuse the iov and cbp
        iov->reset();
        conn->postRecv(iov);
    }

    LOG_TRACE("exit");
    return SUCCESS;

}

int Conn::recvCallback(IoVec *iov, void *param, IoVec::state_t state)
{
    LOG_TRACE("enter\n");
    if (param == NULL)
    {
        LOG_ERROR("param not set properly\n");
        return CONN_ERR_UNAVAIL;
    }
    cb_param_t* cbp = static_cast<cb_param_t*>(param);

    if (state == IoVec::CLEANUP || state == IoVec::ERROR)
    {
        int rc =  recvErrCb(iov, cbp, state, true);

        LOG_TRACE("exit");

        return rc;
    }

    // Conn and CommStage must be set and valid.
    Conn* conn = cbp->conn;
    ASSERT(conn, "bad connection pointer");
    CommStage* cs = (CommStage *)cbp->cs;
    ASSERT(cs, "bad CommStage pointer");

    Conn::nextrecv_t nr = conn->getNextRecv();
    switch (nr)
    {
    case Conn::HEADER:
    {
        int rc = recvHeaderCb(iov, cbp, state);

        LOG_TRACE("exit");
        return rc;

    }
        break;
    case Conn::MESSAGE:
    {

        Message* msg = (Message *)gDeserializable->deserialize((char *)iov->getBase(),
                 (int)iov->getSize());
        if (msg == NULL)
        {
            // Cannot deserialize the message. This may be a message built
            // out of a newer schema. Drain the socket if attachement present
            // If no attachment, repost
            LOG_ERROR("can't deserialize message\n");

            // Send malformed error response
            sendBadMsgErr(conn, (char *)iov->getBase(), CommEvent::MALFORMED_MESSAGE,
                    "Failed to deserialize message");

            u64_t leftSize = cbp->attLen + cbp->fileLen;
            prepare2Drain(iov, cbp, conn, leftSize);

            LOG_TRACE("exit");
            return Conn::CONN_ERR_MISMATCH;
        }
        else if (dynamic_cast<Request *>(msg))
        {
            int rc = recvReqMsg((Request *)msg, iov, cbp, conn);
            if (rc)
            {
                u64_t leftSize = cbp->attLen + cbp->fileLen;
                prepare2Drain(iov, cbp, conn, leftSize);
            }

            LOG_TRACE("exit");
            return rc;

        }
        else if (dynamic_cast<Response *>(msg))
        {
            int rc = recvRspMsg((Response *)msg, iov, cbp, conn);
            if (rc)
            {
                u64_t leftSize = cbp->attLen + cbp->fileLen;
                prepare2Drain(iov, cbp, conn, leftSize);
            }

            LOG_TRACE("exit");
            return rc;

        }
        else
        {
            // Neither request nor message are present. Error.
            LOG_ERROR( "%s", "no response or request in msg");
            u64_t leftSize = cbp->attLen + cbp->fileLen;
            prepare2Drain(iov, cbp, conn, leftSize);

            LOG_TRACE("exit");
            return Conn::CONN_ERR_MISMATCH;
        }

    }
        break;

    case Conn::ATTACHMENT:
    {

        cbp->remainVecs--;
        if (cbp->remainVecs == 0)
        {
            int rc = recvAttach(iov, cbp, conn);
            if (rc)
            {
                u64_t leftSize = cbp->fileLen;
                prepare2Drain(iov, cbp, conn, leftSize);

                LOG_TRACE("exit");
                return rc;
            }

            LOG_TRACE("exit");
            return SUCCESS;
        }
        else
        {
            if (iov->getAllocType() == IoVec::SYS_ALLOC)
            {
                delete (char *)iov->getBase();
            }
            delete iov;

            LOG_TRACE("exit");
            return SUCCESS;
        }
    }
        break;

    case Conn::ATTACHFILE:
    {

        int rc = recvFile(iov, cbp, conn);

        LOG_TRACE("exit");

        return rc;
    }
        break;

    case Conn::DRAIN:
    {

        int rc = recvDrain(iov, cbp, conn);

        LOG_TRACE("exit");
        return rc;
    }
        break;

    default:
        LOG_ERROR("unexpected mNextRecvPart parameter");
        return IoVec::CB_ERROR;
    }


    /**
     * event done place
     */
    LOG_TRACE("exit\n");

    return SUCCESS;
}

int Conn::sendReqCallback(cb_param_t* cbp, IoVec::state_t state)
{
    LOG_TRACE("enter");

    Conn *conn = cbp->conn;
    CommEvent *cev = cbp->cev;

    if (state == IoVec::DONE)
    {
        // On the client side the event is completed when a
        // response message is received.
        cev->setStatus(CommEvent::SUCCESS);
    }
    else if (state == IoVec::CLEANUP || state == IoVec::ERROR)
    {
        conn->removeEventEntry(cbp->reqId);

        CommEvent::status_t evst =
                (state == IoVec::CLEANUP) ?
                        CommEvent::STAGE_CLEANUP : CommEvent::SEND_FAILURE;

        if (cbp->eventDone == false)
        {
            cev->completeEvent(evst);
            cbp->eventDone = true;
        }
    }
    else
    {
        conn->removeEventEntry(cbp->reqId);

        // Unknown iovec completion status
        cev->completeEvent(CommEvent::SEND_FAILURE);
    }

    delete cbp;

    LOG_TRACE("exit");
    return SUCCESS;

}

int Conn::sendRspCallback(cb_param_t* cbp, IoVec::state_t state)
{
    LOG_TRACE("enter");

    CommStage* cs = (CommStage *)cbp->cs;
    ASSERT(cs, "bad CommStage pointer");

    // Event generated at server side is safe to be changed outside lock
    // protection, no race with other thread here
    CommEvent* cev = cbp->cev;
    ASSERT(cev && cev->isServerGen(), "bad CommEvent pointer");

    if (state == IoVec::DONE)
    {
        // If the iovec is completed OK and this is an event generated at
        // the server, only then the event is done.
        cev->done();
    }
    else if (state == IoVec::CLEANUP || state == IoVec::ERROR)
    {
        CommEvent::status_t evst =
                (state == IoVec::CLEANUP) ?
                        CommEvent::STAGE_CLEANUP : CommEvent::SEND_FAILURE;
        cev->completeEvent(evst);
    }
    else
        // Unknown iovec completion status
        cev->completeEvent(CommEvent::SEND_FAILURE);

    delete cbp;

    LOG_TRACE("exit");
    return SUCCESS;
}

int Conn::sendCallback(IoVec *iov, void *param, IoVec::state_t state)
{
    LOG_TRACE("enter");

    char *base = static_cast<char *>(iov->getBase());
    cb_param_t* cbp = static_cast<cb_param_t*>(param);

    // Free the iovec base if it was system allocated
    if (iov->getAllocType() == IoVec::SYS_ALLOC)
        delete[] base;
    delete iov;

    if (!cbp) // Nothing to do here
    {
        LOG_TRACE("exit");
        return SUCCESS;
    }


    //check whether send file
    if (state == IoVec::DONE && cbp->fileLen != 0)
    {
        s64_t sendCount = 0;
        Conn *conn = cbp->conn;
        Sock::sendfile(conn->getSocket(), cbp->filePath,
                cbp->fileOffset, cbp->fileLen, &sendCount);
        if ( sendCount != (s64_t)cbp->fileLen)
        {
            LOG_ERROR("Failed to send ");
            state = IoVec::ERROR;
        }
    }

    CommEvent *cev = cbp->cev;
    if (cev->isServerGen())
    {
        int rc = sendRspCallback(cbp, state);

        LOG_TRACE("exit");
        return rc;
    }
    else
    {
        int rc = sendReqCallback(cbp, state);
        LOG_TRACE("exit");
        return rc;
    }

}

