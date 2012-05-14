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

#include <string.h>
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "lang/lstring.h"
#include "trace/log.h"

#include "time/datetime.h"

#include "net/conn.h"
#include "net/sockutil.h"
#include "net/iovec.h"

#include "comm/commevent.h"
#include "comm/commstage.h"
#include "comm/message.h"
#include "comm/request.h"
#include "comm/response.h"
#include "comm/packageinfo.h"


//! Implementation of Conn
/**
 * @file
 * @author Longda
 * @date   5/08/07
 */



EndPoint Conn::gLocalEp;
Stage* Conn::gCommStage = NULL;

int Conn::gSocketSendBufSize = Sock::SOCK_SEND_BUF_SIZE;
int Conn::gSocketRcvBufSize = Sock::SOCK_RECV_BUF_SIZE;

int Conn::gListenSendBufSize = Sock::SOCK_SEND_BUF_SIZE;
int Conn::gListenRcvBufSize = Sock::SOCK_RECV_BUF_SIZE;

int Conn::gMaxBlockSize    = 64 * ONE_MILLION;
int Conn::gTimeout = Sock::SOCK_TIMEOUT;

Deserializable *Conn::gDeserializable = NULL;
CSelectDir     *Conn::gSelectDir      = NULL;

u64_t Conn::globalActSn = 0;

Conn::Conn() :
        mSock(Sock::DISCONNECTED),
        mNextRecvPart(HEADER),
        mConnState(SUCCESS),
        mCleaning(false),
        mCleanType(ON_CLEANUP),
        mRefCount(0),
        mActivitySn(0),
        mCurSendBlock(NULL),
        mSendQ(),
        mReadyToSend(false),
        mCurRecvBlock(NULL),
        mRecvQ(),
        mReadyRecv(false),
        mRecvCb(NULL)
{
    LOG_TRACE("enter");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    MUTEX_INIT(&mMutex, &attr);
    MUTEX_INIT(&mSendMutex, &attr);
    MUTEX_INIT(&mRecvMutex, &attr);
    MUTEX_INIT(&mEventMapMutex, &attr);

    updateActSn();

    LOG_TRACE("exit");
}

Conn::~Conn()
{
    LOG_TRACE( "%s", "enter");

    ASSERT((mCurSendBlock == 0), "mCurSendBlock is not 0");
    ASSERT((mCurRecvBlock == 0), "mCurRecvBlock is not 0");
    ASSERT((mRefCount == 0), "connection mMsgCounter not 0");

    MUTEX_DESTROY(&mMutex);
    MUTEX_DESTROY(&mSendMutex);
    MUTEX_DESTROY(&mRecvMutex);
    MUTEX_DESTROY(&mEventMapMutex);

    LOG_TRACE( "%s", "exit");
}

Conn::status_t Conn::connect(EndPoint& ep, int &sock)
{
    LOG_TRACE("enter");

    int rc = 0;
    rc = Sock::create(PF_INET, SOCK_STREAM, 0, sock);
    if (rc != Sock::SUCCESS)
    {
        LOG_ERROR("Failed to create socket rc:%d:%s, conn_rc:%d",
                errno, strerror(errno), rc);
        return CONN_ERR_CREATE;
    }

    // setup socket here to get non-blocking connect
    setup(sock);

    rc = Sock::connectTo(sock, ep.getHostName(), ep.getPort());
    if (rc == Sock::SOCK_CONNECTING)
    {
        //This is the most case
        LOG_INFO("connect to %s @ %d is underway",
                ep.getHostName(), ep.getPort());
        return CONN_CONNECTING;
    }
    else if (rc == Sock::SUCCESS)
    {
        LOG_INFO("Successfully connect to %s @ %d",
                ep.getHostName(), ep.getPort());
        return SUCCESS;
    }
    else
    {
        //failed to connect
        LOG_ERROR("connect to %s @ %d failed", ep.getHostName(), ep.getPort());
        return CONN_ERR_CONNECT;
    }

    LOG_TRACE("exit");
    return SUCCESS;
}

void Conn::setup(int sock)
{
    LOG_TRACE( "enter");

    int rv = MUTEX_LOCK(&mMutex);
    ASSERT((rv == 0), "thread already owns mutex");

    this->mSock = sock;
    Sock::setCloExec(sock);
    Sock::setNoDelay(sock);
    Sock::setBufSize(sock, gSocketSendBufSize, gSocketRcvBufSize);
    Sock::setNonBlocking(sock);

    rv = MUTEX_UNLOCK(&mMutex);
    ASSERT((rv == 0), "%s", "thread does not own mutex");

    LOG_TRACE( "exit");
}

Conn::status_t Conn::disconnect()
{
    LOG_TRACE("enter");

    status_t rc = SUCCESS;

    if (!cleanup(ON_CLEANUP))
        rc = CONN_ERR_DISCONNECT;

    if (rc == SUCCESS)
    {
        LOG_INFO("Successfully disconnect %s@%d",
                mPeerEp.getHostName(), mPeerEp.getPort());
    }
    LOG_TRACE("exit");

    return rc;
}

void Conn::cleanupVec(IoVec *iov, cleanup_t how)
{
    LOG_TRACE( "%s", "enter");
    if (iov)
    {
        IoVec::callback_t cb = iov->getCallback();
        // Let the callback decide what to do with the vector
        // We cannot just free iov's memory, because its base pointer
        // may be allocated on the heap and this memory will be lost
        IoVec::state_t st = (how == ON_CLEANUP) ? IoVec::CLEANUP : IoVec::ERROR;
        if (cb)
            (cb)(iov, iov->getCallbackParam(), st);
    }
    LOG_TRACE( "%s", "exit");
}

int Conn::cleanup(cleanup_t how)
{
    LOG_TRACE("enter");
    int rv;

    rv = MUTEX_LOCK(&mMutex);
    ASSERT((rv == 0), "thread already owns mutex");

    // 2. Indicate intention to cleanup connection. This will also tell us
    // to stop sending and receiving from the conn socket
    mCleaning = true;

    // 3. Check if anybody else is using conn. If yes, can't cleanup now
    // Return false and let the conn mgr decide how to cleaup this conn
    if (mRefCount > 0)
    {
        // Save cleanup type so we know how to cleanup the connection when
        // refcnt reaches zero
        mCleanType = how;

        rv = MUTEX_UNLOCK(&mMutex);
        ASSERT((rv == 0), "thread does not own mutex");

        LOG_INFO("Prepare to cleanup %s:%d connection, but not now",
                mPeerEp.getHostName(), mPeerEp.getPort());

        LOG_TRACE("exit");

        return CONN_ERR_BUSY;
    }

    rv = MUTEX_UNLOCK(&mMutex);
    ASSERT((rv == 0), "thread does not own mutex");

    // Clenaup remaining mRecvQ iovecs and mCurRecvBlock
    rv = MUTEX_LOCK(&mRecvMutex);
    ASSERT((rv == 0), "thread already owns mutex");

    while (!mRecvQ.empty())
    {
        cleanupVec(mRecvQ.front(), how);
        mRecvQ.pop_front();
    }
    cleanupVec(mCurRecvBlock, how);
    mCurRecvBlock = 0;
    rv = MUTEX_UNLOCK(&mRecvMutex);
    ASSERT((rv == 0), "thread already release mutex");

    // Clenaup remaining mSendQ iovecs and mCurSendBlock
    rv = MUTEX_LOCK(&mSendMutex);
    ASSERT(rv == 0, "thread does not own mutex");

    while (!mSendQ.empty())
    {
        cleanupVec(mSendQ.front(), how);
        mSendQ.pop_front();
    }
    cleanupVec(mCurSendBlock, how);
    mCurSendBlock = 0;

    rv = MUTEX_UNLOCK(&mSendMutex);
    ASSERT(rv == 0, "thread does not own mutex");

    // Call disconnect callback set by Net
    int rc = SUCCESS;
    rc = connCallback(Conn::ON_DISCONNECT);

    if (mSock != Sock::DISCONNECTED)
    {
        close(mSock);
        mSock = Sock::DISCONNECTED;
    }

    LOG_INFO("Successfully cleanup connection %s:%d, %p",
            mPeerEp.getHostName(), mPeerEp.getPort(), this);

    delete this;

    LOG_TRACE("exit");

    return rc;
}

//! Release connection
/**
 * Decrements reference mMsgCounter of connection. Allows for clean removal
 * of conn objects when multiple threads keep references.
 *
 * NOTE: Adds a substantial overhead. Needs to be looked at later.
 */
void Conn::release()
{
    LOG_TRACE("enter");

    bool callCleanup = false;

    int rv = MUTEX_LOCK(&mMutex);
    ASSERT((rv == 0), "thread already owns mutex");

    mRefCount--;

    LOG_DEBUG("Release connection %s:%d, conf count:%d, %p",
                mPeerEp.getHostName(), mPeerEp.getPort(), mRefCount, this);

    if (mRefCount == 0 && mCleaning)
        callCleanup = true;

    rv = MUTEX_UNLOCK(&mMutex);
    ASSERT((rv == 0), "thread does not own mutex");

    if (callCleanup)
        cleanup(mCleanType);

    LOG_TRACE("exit");
}

//! Acquire connection
/**
 * Incrments reference mMsgCounter of connection. Allows for clean removal
 * of conn objects when multiple threads keep references.
 *
 * NOTE: Adds a substantial overhead. Needs to be looked at later.
 */
void Conn::acquire()
{
    LOG_TRACE( "%s", "enter");

    int rv = MUTEX_LOCK(&mMutex);
    ASSERT((rv == 0), "thread already owns mutex");

    mRefCount++;

    rv = MUTEX_UNLOCK(&mMutex);
    ASSERT((rv == 0), "thread does not own mutex");

    LOG_DEBUG("Acquire connection %s:%d, conf count:%d, %p",
                    mPeerEp.getHostName(), mPeerEp.getPort(), mRefCount, this);

    LOG_TRACE( "%s", "exit");
}

bool Conn::isIdle()
{
    int rv = MUTEX_LOCK(&mMutex);
    ASSERT((rv == 0), "thread already owns mutex");

    bool idle = (mRefCount == 0);

    rv = MUTEX_UNLOCK(&mMutex);
    ASSERT((rv == 0), "thread does not own mutex");

    return idle;
}

Conn::status_t Conn::send(int numVecs, IoVec* msgVecs[])
{
    LOG_TRACE( "enter");

    // As an optimization, here we can start sending the iovecs directly
    // before they are queued to the mSendQ. The ones that are completed
    // will be marked and their callback invoked. The rest will be queued.
    postSend(numVecs, msgVecs);

    if (mConnState == CONN_CONNECTING)
        return SUCCESS;

    status_t rc = sendProgress();

    LOG_TRACE("exit");

    return rc;
}

Conn::status_t Conn::send(IoVec* msgVec)
{
    status_t rc = send(1, &msgVec);

    return rc;
}

Conn::status_t Conn::postSend(int numVecs, IoVec* msgVecs[])
{
    int rv = MUTEX_LOCK(&mSendMutex);
    for (int i = 0; i < numVecs; i++)
    {
        mSendQ.push_back(msgVecs[i]);
    }
    if (rv == 0)
        MUTEX_UNLOCK(&mSendMutex);

    return SUCCESS;
}

Conn::status_t Conn::postSend(IoVec* msgVec)
{
    int rv = MUTEX_LOCK(&mSendMutex);

    mSendQ.push_back(msgVec);

    if (rv == 0)
        MUTEX_UNLOCK(&mSendMutex);

    return SUCCESS;
}

Conn::status_t Conn::recv(int numVecs, IoVec* msgVecs[])
{
    LOG_TRACE("Enter");

    postRecv(numVecs, msgVecs);

    status_t rc = recvProgress();

    LOG_TRACE("Exit");
    return rc;
}

Conn::status_t Conn::postRecv(int numVecs, IoVec* msgVecs[])
{
    int rv = MUTEX_LOCK(&mRecvMutex);

    for (int i = 0; i < numVecs; i++)
    {
        mRecvQ.push_back(msgVecs[i]);
    }

    if (rv == 0)
        MUTEX_UNLOCK(&mRecvMutex);

    return SUCCESS;
}

Conn::status_t Conn::postRecv(IoVec* msgVec)
{
    int rv = MUTEX_LOCK(&mRecvMutex);

    mRecvQ.push_back(msgVec);

    if (rv == 0)
        MUTEX_UNLOCK(&mRecvMutex);

    return SUCCESS;
}

Conn::status_t Conn::sendvecProgress()
{
    int nw = 0;
    status_t rc;
    IoVec::state_t vs;

    if (!mCurSendBlock)
    {
        return CONN_ERR_NOVEC;
    }
    if (!mReadyToSend)
    { // This socket is not marked as available yet
        return CONN_ERR_UNAVAIL;
    }

    while (!mCurSendBlock->done())
    {
        int rv = MUTEX_LOCK(&mMutex);
        ASSERT((rv == 0), "thread already owns mutex");

        // Check if the connection is not in cleanup. If it is, we should
        // not attempt to send. After we have gotten the
        // notification in the RecvThread throuth the receive side epoll fd
        // we are getting SIGPIPE in a subsequent write to the same
        // socket and the process silently dies
        bool toSend = !mCleaning;

        rv = MUTEX_UNLOCK(&mMutex);
        ASSERT((rv == 0), "thread does not own mutex");

        if (toSend)
        {
            nw = ::send(mSock, mCurSendBlock->curPtr(), mCurSendBlock->remain(),
                    MSG_NOSIGNAL);
            LOG_DEBUG("::send by socket, %d", mSock);
        }
        else
        {
            LOG_ERROR("nw = 0;");
            nw = 0;
        }

        if (nw > 0)
            mCurSendBlock->incXferred(nw); // Increment with the sent chunk size
        else
            break;
    }

    if (mCurSendBlock->done())
    {
        vs = IoVec::DONE;
        rc = SUCCESS;
    }
    else if (nw < 0 && errno == EAGAIN)
    {
        mReadyToSend = false;         // Can't send any more but vector not done
        rc = CONN_ERR_UNAVAIL;
    }
    else if (nw < 0 && errno == EPIPE)
    {
        LOG_ERROR("broken socket");
        vs = IoVec::ERROR;
        rc = CONN_ERR_BROKEN;
    }
    else if (nw == 0)
    {
        // This can only happen if the connection is broken and this is
        // discovered by the receive thread. We just need to cleanly release
        // the current reference to the connection object, so that the
        // cleanup initiated by the receive thread can complete OK
        vs = IoVec::ERROR;
        rc = CONN_ERR_BROKEN;
    }
    else
    {
        LOG_ERROR("unexpected condition: %s", strerror(errno));
        vs = IoVec::ERROR;
        rc = CONN_ERR_BROKEN;
    }

    if (rc != SUCCESS && rc != CONN_ERR_UNAVAIL)
    {
        LOG_ERROR("Failed to send data to %s@%d, rc %d:%s, conn_rc:%d",
                mPeerEp.getHostName(), mPeerEp.getPort(), errno, strerror(errno), rc);
    }

    // In all cases except when mCurSendBlock is not done and there is no error
    // on the connection -> complete mCurSendBlock
    if (rc != CONN_ERR_UNAVAIL)
    {
        IoVec* svec = mCurSendBlock;
        // Clean up current vector, it's given to callback.
        // mCurSendBlock should not be referenced in this context from now on
        mCurSendBlock = 0;
        IoVec::callback_t cb = svec->getCallback();
        if (cb)
        {
            (cb)(svec, svec->getCallbackParam(), vs);
        }
    }

    return rc;
}

Conn::status_t Conn::sendProgress(bool ready)
{
    status_t rc;
    bool sendqEmpty = false;

    do
    {
        int rv = MUTEX_LOCK(&mSendMutex);
        ASSERT((rv == 0), "thread already owns mutex");
        if (mReadyToSend || ready)
        {
            mReadyToSend = true;
            rc = sendvecProgress();
        }
        else
            rc = CONN_ERR_UNAVAIL;

        // if CONN_ERR_NOVEC returned, try to get one from the queue

        if (rc == CONN_ERR_BROKEN)
        {
            rv = MUTEX_UNLOCK(&mSendMutex);
            ASSERT((rv == 0), "thread does not own mutex");
            LOG_ERROR("connection is broken");
            return rc;
        }

        if (rc == CONN_ERR_UNAVAIL)
        {
            rv = MUTEX_UNLOCK(&mSendMutex);
            ASSERT((rv == 0), "%s", "thread does not own mutex");
            break;  // Can't send any more, socket full, mCurSendBlock remains
        }

        // Here if mCurSendBlock empty, possibly after completion of prev mCurSendBlock
        if (mSendQ.size() > 0)
        {
            ASSERT((mCurSendBlock == 0), "mCurSendBlock is not 0");
            mCurSendBlock = mSendQ.front();
            mSendQ.pop_front();
        }
        else
            sendqEmpty = true;
        rv = MUTEX_UNLOCK(&mSendMutex);
        ASSERT((rv == 0), "%s", "thread does not own mutex");
    } while (!sendqEmpty);
    // We get here if:
    // 1. There are no more send iovec's
    // 2. The send socket is full, mCurSendBlock may be 0 or active

    return SUCCESS;
}

Conn::status_t Conn::recvvecProgress()
{
    int nr = 0;
    status_t rc;
    IoVec::state_t vs;

    if (!mCurRecvBlock)
    {
        return CONN_ERR_NOVEC;
    }
    if (!mReadyRecv)
    { // This socket is not marked as available yet
        return CONN_ERR_UNAVAIL;
    }

    while (!mCurRecvBlock->done())
    {
        int rv = MUTEX_LOCK(&mMutex);
        ASSERT((rv == 0), "thread already owns mutex");

        bool toRecv = !mCleaning;

        rv = MUTEX_UNLOCK(&mMutex);
        ASSERT((rv == 0), "thread does not own mutex");

        if (toRecv)
            nr = ::recv(mSock, mCurRecvBlock->curPtr(), mCurRecvBlock->remain(),
                    0);
        else
            nr = 0;

        if (nr > 0)
            mCurRecvBlock->incXferred(nr); // Increment with the receive chunk size
        else
            break;
    }

    if (mCurRecvBlock->done())
    {
        rc = SUCCESS;
        vs = IoVec::DONE;
    }
    else if (nr < 0 && errno == EAGAIN)
    {
        mReadyRecv = false;    // Can't receive any more but vector not done
        rc = CONN_ERR_UNAVAIL;
    }
    else if (nr == 0)
    {
        LOG_ERROR("connection broken");
        vs = IoVec::ERROR;
        rc = CONN_ERR_BROKEN;
    }
    else
    {
        LOG_ERROR("unexpected condition");
        vs = IoVec::ERROR;
        rc = CONN_ERR_BROKEN;
    }

    if (rc != SUCCESS && rc != CONN_ERR_UNAVAIL)
    {
        LOG_ERROR("Failed to receive data from %s@%d, rc %d:%s, conn_rc:%d",
                mPeerEp.getHostName(), mPeerEp.getPort(), errno, strerror(errno), rc);
    }

    if (rc != CONN_ERR_UNAVAIL)
    {
        IoVec *rvec = mCurRecvBlock;
        // !! It is important to set mCurRecvBlock = 0 before the callback
        mCurRecvBlock = 0; // clean up current vector, it's given to the callback
        IoVec::callback_t cb = rvec->getCallback();
        if (cb)
        {
            (cb)(rvec, rvec->getCallbackParam(), vs);
        }
        else
        {
            LOG_INFO("no callback in receive IoVec");
        }
    }

    return rc;
}

Conn::status_t Conn::recvProgress(bool ready)
{
    status_t rc;
    bool recvqEmpty = false;
    int nvecs = 0;

    updateActSn();

    do
    {
        int rv = MUTEX_LOCK(&mRecvMutex);
        ASSERT((rv == 0), "thread already owns mutex");
        if (mReadyRecv || ready)
        {
            mReadyRecv = true;
            rc = recvvecProgress();
        }
        else
            rc = CONN_ERR_UNAVAIL;

        if (rc == CONN_ERR_BROKEN)
        {
            rv = MUTEX_UNLOCK(&mRecvMutex);
            ASSERT((rv == 0), "thread does not own mutex");
            LOG_DEBUG( "connection broken: exit");
            return rc;
        }

        if (rc == CONN_ERR_UNAVAIL)
        {
            rv = MUTEX_UNLOCK(&mRecvMutex);
            ASSERT((rv == 0), "thread does not own mutex");
            break;
        }

        if (!mRecvQ.empty())
        {
            ASSERT((mCurRecvBlock == 0), "%s", "mCurRecvBlock not set to 0");
            mCurRecvBlock = mRecvQ.front();
            mRecvQ.pop_front();
        }
        else
            recvqEmpty = true;

        if (rc != CONN_ERR_NOVEC)
            nvecs++;

        MUTEX_UNLOCK(&mRecvMutex);
    } while (!recvqEmpty && nvecs < VEC_BATCH_NUM);

    return (mReadyRecv & (mCurRecvBlock != 0)) ? CONN_READY : SUCCESS;
}

int Conn::getSocket()
{
    MUTEX_LOCK(&mMutex);
    int s = mSock;
    MUTEX_UNLOCK(&mMutex);
    return s;
}

void Conn::setSock(int sock)
{
    MUTEX_LOCK(&mMutex);
    mSock = sock;
    MUTEX_UNLOCK(&mMutex);
    return ;
}

bool Conn::connected()
{
    MUTEX_LOCK(&mMutex);
    bool connflag = (mSock != Sock::DISCONNECTED);
    MUTEX_UNLOCK(&mMutex);
    return connflag;
}

void Conn::setState(Conn::status_t state)
{
    MUTEX_LOCK(&mMutex);
    mConnState = state;
    MUTEX_UNLOCK(&mMutex);
}

Conn::status_t Conn::getState()
{
    MUTEX_LOCK(&mMutex);
    Conn::status_t state = mConnState;
    MUTEX_UNLOCK(&mMutex);

    return state;
}

void Conn::setNextRecv(nextrecv_t nr)
{
    MUTEX_LOCK(&mMutex);
    mNextRecvPart = nr;
    MUTEX_UNLOCK(&mMutex);
}

Conn::nextrecv_t Conn::getNextRecv()
{
    MUTEX_LOCK(&mMutex);
    nextrecv_t nr = mNextRecvPart;
    MUTEX_UNLOCK(&mMutex);
    return nr;
}

void Conn::messageOut()
{
    MUTEX_LOCK(&mMutex);
    mMsgCounter.out++;
    MUTEX_UNLOCK(&mMutex);
}

void Conn::messageIn()
{
    MUTEX_LOCK(&mMutex);
    mMsgCounter.in++;
    MUTEX_UNLOCK(&mMutex);
}

void Conn::updateActSn()
{
    // globalActSn is accessed without mutex protection on purpose
    // as it does not need to be 100% accurate
    mActivitySn = ++globalActSn;
}

void Conn::setSndBufSz(int size)
{
    gSocketSendBufSize = size;
}

void Conn::setRcvBufSz(int size)
{
    gSocketRcvBufSize = size;
}

int Conn::getSndBufSz()
{
    return gSocketSendBufSize;
}

int Conn::getRcvBufSz()
{
    return gSocketRcvBufSize;
}

void Conn::setLsnSndBufSz(int size)
{
    gListenSendBufSize = size;
}

void Conn::setLsnRcvBufSz(int size)
{
    gListenRcvBufSize = size;
}

int Conn::getLsnSndBufSz()
{
    return gListenSendBufSize;
}

int Conn::getLsnRcvBufSz()
{
    return gListenRcvBufSize;
}

int Conn::getSocketTimeout()
{
    return gTimeout;
}

void Conn::setSocketTimeout(int timeout)
{
    gTimeout = timeout;
}

void Conn::setMaxBlockSize(int size)
{
    gMaxBlockSize = size;
}

int Conn::getMaxBlockSize()
{
    return gMaxBlockSize;
}

void Conn::setSocketProperty(std::map<std::string, std::string> &section)
{
    std::map<std::string, std::string>::iterator it;

    std::string key;

    key = "socket_timeout";
    it = section.find(key);
    if (it != section.end())
    {
        int timeout = 30;
        CLstring::strToVal(it->second, timeout);

        Conn::setSocketTimeout(timeout);

        LOG_INFO("Setting socket timeout as %d", timeout);
    }
    else
    {
        LOG_INFO("Setting socket timeout as %d", Conn::getSocketTimeout());
    }

    key = "socket_send_buf_size";
    it = section.find(key);
    if (it != section.end())
    {
        int socket_send_buf_size = 262144;
        CLstring::strToVal(it->second, socket_send_buf_size);

        Conn::setSndBufSz(socket_send_buf_size);

        LOG_INFO("Setting socket send buffer size as %d", socket_send_buf_size);
    }
    else
    {
        LOG_INFO("Setting socket send buffer size as %d", Conn::getSndBufSz());
    }

    key = "socket_rcv_buf_size";
    it = section.find(key);
    if (it != section.end())
    {
        int socket_rcv_buf_size = 30;
        CLstring::strToVal(it->second, socket_rcv_buf_size);

        Conn::setRcvBufSz(socket_rcv_buf_size);

        LOG_INFO("Setting socket receive buffer size as %d",
                socket_rcv_buf_size);
    }
    else
    {
        LOG_INFO("Setting socket receive buffer size as %d",
                Conn::getRcvBufSz());
    }

    key = "listen_send_buf_size";
    it = section.find(key);
    if (it != section.end())
    {
        int listen_send_buf_size = 262144;
        CLstring::strToVal(it->second, listen_send_buf_size);

        Conn::setLsnSndBufSz(listen_send_buf_size);

        LOG_INFO("Setting listen socket send buffer size as %d",
                listen_send_buf_size);
    }
    else
    {
        LOG_INFO("Setting listen socket send buffer size as %d",
                Conn::getLsnSndBufSz());
    }

    key = "listen_rcv_buf_size";
    it = section.find(key);
    if (it != section.end())
    {
        int listen_rcv_buf_size = 30;
        CLstring::strToVal(it->second, listen_rcv_buf_size);

        Conn::setLsnRcvBufSz(listen_rcv_buf_size);

        LOG_INFO("Setting listen socket receive buffer size as %d",
                listen_rcv_buf_size);
    }
    else
    {
        LOG_INFO("Setting listen socket receive buffer size as %d",
                Conn::getLsnRcvBufSz());
    }

    key = "one_block_buffer_size";
    it = section.find(key);
    if (it != section.end())
    {
        int one_block_buffer_size = 30;
        CLstring::strToVal(it->second, one_block_buffer_size);

        Conn::setMaxBlockSize(one_block_buffer_size);

        LOG_INFO("Setting max block buffer size as %d",
                one_block_buffer_size);
    }
    else
    {
        LOG_INFO("Setting max block buffer size as %d",
                Conn::getMaxBlockSize());
    }

    return;
}

void Conn::setDeserializable(Deserializable *deserializer)
{
    gDeserializable = deserializer;
}
Deserializable * Conn::getDeserializable()
{
    return gDeserializable;
}

void Conn::setSelectDir(CSelectDir *selectDir)
{
    gSelectDir = selectDir;
}

CSelectDir *Conn::getSelectDir()
{
    return gSelectDir;
}

void Conn::setCommStage(Stage *commStage)
{
    gCommStage = commStage;
}

bool Conn::setLocalEp(std::map<std::string, std::string> &section, bool server)
{
    std::map<std::string, std::string>::iterator it;

    std::string key;

    key = "port";
    it = section.find(key);
    if (it != section.end())
    {
        s32_t port = 0;
        CLstring::strToVal(it->second, port);
        gLocalEp.setPort(port);
    }

    if (server == true && gLocalEp.getPort() == EndPoint::DEFAULT_PORT)
    {
        LOG_ERROR("Haven't set port, but it is one server");
        return false;
    }

    key = "location";
    it = section.find(key);
    if (it != section.end())
    {
        gLocalEp.setLocation(it->second.c_str());
    }

    key = "service";
    it = section.find(key);
    if (it != section.end())
    {
        gLocalEp.setService(it->second.c_str());
    }

    std::string output;
    gLocalEp.toString(output);

    LOG_INFO("local endpont is %s", output.c_str());

    return true;
}

EndPoint& Conn::getLocalEp()
{
    return gLocalEp;
}

void Conn::addEventEntry(u32_t msgId, CommEvent *event)
{
    MUTEX_LOCK(&mEventMapMutex);

    mSendEventMap.insert(std::pair<u32_t, CommEvent *>(msgId, event));

    MUTEX_UNLOCK(&mEventMapMutex);

}

void Conn::removeEventEntry(u32_t msgId)
{
    MUTEX_LOCK(&mEventMapMutex);

    mSendEventMap.erase(msgId);

    MUTEX_UNLOCK(&mEventMapMutex);
}

CommEvent* Conn::getAndRmEvent(u32_t msgId)
{
    CommEvent* ret = NULL;
    MUTEX_LOCK(&mEventMapMutex);

    std::map<u32_t, CommEvent*>::iterator it = mSendEventMap.find(msgId);
    if (it != mSendEventMap.end())
    {
        ret = it->second;
        mSendEventMap.erase(it);
    }
    MUTEX_UNLOCK(&mEventMapMutex);

    return ret;
}

