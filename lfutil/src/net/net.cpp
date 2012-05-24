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

#include <iostream>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "trace/log.h"
#include "os/mutex.h"
#include "lang/lstring.h"

#include "net/net.h"
#include "net/netserver.h"
#include "net/netex.h"
#include "net/sockutil.h"
#include "net/conn.h"
#include "net/iovec.h"
#include "net/connmgr.h"
#include "comm/commdataevent.h"
#include "conf/ini.h"

//! Implementation of Net
/**
 * @file
 * @author Longda
 * @date   5/05/07
 * 
 */

#define INFORM_WITH_SIGNAL  0

typedef struct _DataThreadInputParam
{
    Net             *netInstance;
    int              threadIndex;
} DataThreadInputParam;

Net::Net(Stage *commStage) :
        initFlag(false), shutdownFlag(false), connMgr(), mCommStage(commStage)

{
    LOG_TRACE("enter");
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    pthread_mutex_init(&netMutex, &attr);
    LOG_TRACE("exit");
}

Net::~Net()
{
    LOG_TRACE("enter");
    ASSERT((!initFlag || shutdownFlag), "Net is not finalized properly");
    pthread_mutex_destroy(&netMutex);
    LOG_TRACE("exit");
}

int Net::setupSelectors()
{
    int rc;

    // Create notification pipe for receiver thread
    rc = pipe(recvPfd);
    if (rc < 0)
    {

        LOG_ERROR("reader pipe failed, %d:%s", rc, strerror(rc));
        return rc;
    }

    // Create a selector for receiver side of sockets
    rc = Sock::createSelector(recvPfd[0], recvEpfd);
    if (rc != Sock::SUCCESS)
    {

        LOG_ERROR("recv selector creation failed");
        return rc;
    }

    // Create notification pipe for sender thread
    rc = pipe(sendPfd);
    if (rc < 0)
    {
        LOG_ERROR("writer pipe failed, %d:%s", rc, strerror(rc));
        return rc;
    }

    // Create a selector for sender side of sockets
    rc = Sock::createSelector(sendPfd[0], sendEpfd);
    if (rc != Sock::SUCCESS)
    {

        LOG_ERROR("send selector creation failed");
        return rc;
    }

    return 0;
}

int Net::startDataThread(int threadIndex, bool isSend)
{
    DataThreadParam *dataTParam = new DataThreadParam();
    if (dataTParam == NULL)
    {
        LOG_ERROR("Failed to new DataThreadParam");
        return -1;
    }

    DataThreadInputParam *threadParam = new DataThreadInputParam();
    if (threadParam == NULL)
    {
        LOG_ERROR("Failed to new DataThreadInputParam");
        delete dataTParam;
        return -1;
    }
    threadParam->netInstance = this;
    threadParam->threadIndex = threadIndex;

    if (isSend)
    {
        int rc = pthread_create(&dataTParam->tid, NULL, SendThread, threadParam);
        if (rc)
        {
            LOG_ERROR("Failed to create send data thread, %d:%s",
                    rc, strerror(rc));
            delete threadParam;
            delete dataTParam;
            return rc;
        }

        sendDataThreads.push_back(dataTParam);

        LOG_INFO("Successfully start %d send data thread", threadIndex);

        return 0;
    }
    else
    {
        int rc = pthread_create(&dataTParam->tid, NULL, RecvThread, threadParam);
        if (rc)
        {
            LOG_ERROR("Failed to create recv data thread, %d:%s",
                    rc, strerror(rc));
            delete threadParam;
            delete dataTParam;
            return rc;
        }

        recvDataThreads.push_back(dataTParam);

        LOG_INFO("Successfully start %d recv data thread", threadIndex);

        return 0;
    }

}

int Net::startThreads()
{
    int rc;
    rc = pthread_create(&recvThreadId, NULL, RecvEPollThread, this);
    if (rc != 0)
    {
        LOG_ERROR("create recv epoll thread failed, %d:%s", rc, strerror(rc));
        return rc;
    }
    rc = pthread_create(&sendThreadId, NULL, SendEPollThread, this);
    if (rc != 0)
    {
        LOG_ERROR("create send thread epoll failed, %d:%s", rc, strerror(rc));
        return rc;
    }

    std::string key = "NetThreadCount";
    std::string netThreadStr = theGlobalProperties()->get(key, "8", "Default");
    int         netThreadCount;
    CLstring::strToVal(netThreadStr, netThreadCount);

    for (int i = 0; i < netThreadCount; i++)
    {
        rc = startDataThread(i, true);
        if (rc)
        {
            LOG_ERROR("Failed to create send data thread %d", i);
            return rc;
        }

        rc = startDataThread(i, false);
        if (rc)
        {
            LOG_ERROR("Failed to create recv data thread %d", i);
            return rc;
        }
    }

    LOG_INFO("Successfully create network threads");
    return 0;
}

int Net::setup()
{
    int rc;

    LOG_TRACE("enter");

    MUTEX_LOCK(&netMutex);
    if (initFlag)
    {
        MUTEX_UNLOCK(&netMutex);

        LOG_WARN("Net has been already setup");
        return 0;
    }

    rc = setupSelectors();
    if (rc != SUCCESS)
    {
        MUTEX_UNLOCK(&netMutex);
        LOG_ERROR("setup selectors failed");
        return NET_ERR_EPOLL;
    }

    rc = startThreads();
    if (rc != SUCCESS)
    {
        LOG_ERROR("start threads failed");

        MUTEX_UNLOCK(&netMutex);
        return NET_ERR_THREAD;
    }

    initFlag = true;

    MUTEX_UNLOCK(&netMutex);

    LOG_INFO("Successfully setup net");

    return 0;;
}

int Net::shutdown()
{
    LOG_TRACE("enter");

    MUTEX_LOCK(&netMutex);
    if (!initFlag || shutdownFlag)
    {
        MUTEX_UNLOCK(&netMutex);
        LOG_ERROR("Havn't init or already shutdown");
        return -EINVAL;
    }

    cleanupThreads();

    char thInfo[THREAD_INFO_LEN] =
    { 0 };
    strncpy(thInfo, THREAD_INFO_EXIT, THREAD_INFO_LEN - 1);

    ssize_t s = write(sendPfd[1], (const char *) thInfo, THREAD_INFO_LEN);
    s = write(recvPfd[1], (const char *) thInfo, THREAD_INFO_LEN);

    pthread_join(recvThreadId, NULL);
    pthread_join(sendThreadId, NULL);

    close(recvPfd[0]);
    close(recvPfd[1]);
    close(recvEpfd);

    close(sendPfd[0]);
    close(sendPfd[1]);
    close(sendEpfd);

    shutdownFlag = true;
    initFlag = false;

    MUTEX_UNLOCK(&netMutex);

    LOG_INFO("Successfully shutdown net");

    return 0;
}

void Net::cleanupThreads()
{
    for(u32_t i = 0; i < sendDataThreads.size(); i++)
    {
        DataThreadParam *dataParam = sendDataThreads[i];

        MUTEX_LOCK(&dataParam->mutex);
        dataParam->sockQ.push_back(-1);
#if INFORM_WITH_SIGNAL
        COND_SIGNAL(&dataParam->cond);
#endif
        MUTEX_UNLOCK(&dataParam->mutex);
    }

    for(u32_t i = 0; i < recvDataThreads.size(); i++)
    {
        DataThreadParam *dataParam = recvDataThreads[i];

        MUTEX_LOCK(&dataParam->mutex);
        dataParam->sockQ.push_back(-1);
#if INFORM_WITH_SIGNAL
        COND_SIGNAL(&dataParam->cond);
#endif
        MUTEX_UNLOCK(&dataParam->mutex);
    }

    for(u32_t i = 0; i < sendDataThreads.size(); i++)
    {
        DataThreadParam *dataParam = sendDataThreads[i];

        pthread_join(dataParam->tid, NULL);

        delete dataParam;
    }
    sendDataThreads.clear();

    for(u32_t i = 0; i < recvDataThreads.size(); i++)
    {
        DataThreadParam *dataParam = recvDataThreads[i];

        pthread_join(dataParam->tid, NULL);

        delete dataParam;
    }
    recvDataThreads.clear();

    return ;
}


void Net::sendData(int sock)
{
    LOG_TRACE("enter");

    /**
     * @@@ Testing code
     */
    //connMgr.lock();
    MUTEX_LOCK(&connMgr.mapMutex);
    Conn* conn = connMgr.find(sock);
    //connMgr.unlock();
    MUTEX_UNLOCK(&connMgr.mapMutex);
    if (conn == NULL)
    {
        LOG_INFO("conn has been removed");

        return;
    }

    bool  exception = false;
    try{
        conn->sendProgress();
    }catch(...)
    {
        exception = true;
        LOG_ERROR("Occur exception");
    }

    //conn has already been acquire in Net::SendThread
    conn->release();

    if (exception)
    {
        removeConn(sock);
    }

    LOG_TRACE("exit");
    return;
}

void Net::prepareSend(int sock, Conn *conn)
{
    LOG_TRACE("Enter");
    if (conn)
    {
        LOG_DEBUG("Connection is ready to sending");
        conn->setReadyToSend(true);
    }

    int threadIndex = sock % sendDataThreads.size();
    DataThreadParam *dataParam = sendDataThreads[threadIndex];

    MUTEX_LOCK(&dataParam->mutex);
    dataParam->sockQ.push_back(sock);
#if INFORM_WITH_SIGNAL
    COND_SIGNAL(&dataParam->cond);
#endif
    MUTEX_UNLOCK(&dataParam->mutex);

    LOG_TRACE("Exit");
}

void*
Net::SendEPollThread(void *arg)
{
    Net* net = static_cast<Net*>(arg);

    int epfd = net->sendEpfd;
    int notifyFd = net->sendPfd[0];
    ConnMgr* cm = &net->connMgr;

    int nfds, fd, i;
    bool exitCmd;

    struct epoll_event* events = new struct epoll_event[MAX_EPOLL_EVENTS];

    LOG_INFO("Start network send epoll thread");

    while (true)
    {
        nfds = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);

        exitCmd = false;

        // We need a scheme to ensure fairness of fd processing. Starting
        // always from event[0] allows for potential starvation.
        for (i = 0; i < nfds; i++)
        {
            fd = events[i].data.fd;
            if (fd == notifyFd)
            {
                char info[THREAD_INFO_LEN];
                int nread;

                Sock::readBlocking(notifyFd, info, THREAD_INFO_LEN, nread);

                LOG_INFO("received info signal: %s", info);

                // Record exit command but process all events before exiting
                if (strcmp(info, THREAD_INFO_EXIT) == 0)
                    exitCmd = true;
                continue;
            }

            /**
             * @@@ testing code
             */
            //cm->lock();
            MUTEX_LOCK(&cm->mapMutex);
            Conn* conn = cm->find(fd);
            if (conn == NULL)
            {
                LOG_INFO("conn has been removed");

                net->delSendSelector(fd);
                //cm->unlock();
                MUTEX_UNLOCK(&cm->mapMutex);
                continue;
            }

            // Check for error events
            if (events[i].events & (EPOLLHUP | EPOLLERR))
            {
                // Error on socket - remove the corresponding conn
                LOG_ERROR("detected broken inet socket %s:%d - removing conn",
                        conn->getPeerEp().getHostName(), conn->getPeerEp().getPort());


                conn->release();
                //cm->unlock();
                MUTEX_UNLOCK(&cm->mapMutex);
                net->removeConn(fd);
                continue;
            }

            // check connect result
            if (conn->getState() == Conn::CONN_CONNECTING)
            {
                int error = 0;
                socklen_t len = sizeof(error);

                getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
                if (error)
                {
                    LOG_ERROR("detect connect error, removing conn, socket error:%d",
                            error);
                    conn->release();
                    //cm->unlock();
                    MUTEX_UNLOCK(&cm->mapMutex);
                    net->removeConn(fd);
                    continue;
                }

                LOG_INFO("connecting is finished OK...");
                conn->setState(Conn::CONN_READY);
            }

            net->prepareSend(fd, conn);

            conn->release();
            //cm->unlock();
            MUTEX_UNLOCK(&cm->mapMutex);

        }

        // Check whether the thread has received exit notification
        if (exitCmd)
            break;
    }

    delete[] events;

    LOG_INFO("exit send thread");

    pthread_exit(0);
}

void Net::sending(int threadIndex)
{
    LOG_INFO("%d send data thread has been started", threadIndex);

    DataThreadParam * dataTParam = sendDataThreads[threadIndex];

#if INFORM_WITH_SIGNAL
    MUTEX_LOCK(&dataTParam->mutex);

    while(true)
    {

        COND_WAIT(&dataTParam->cond, &dataTParam->mutex);
        if (dataTParam->sockQ.empty())
        {
            continue;
        }

        while(dataTParam->sockQ.empty() == false)
        {
            int sock = dataTParam->sockQ.front();
            dataTParam->sockQ.pop_front();
            MUTEX_UNLOCK(&dataTParam->mutex);

            if (sock < 0)
            {
                LOG_INFO("%d thread receive quit signal", threadIndex);
                return ;
            }

            sendData(sock);
            MUTEX_LOCK(&dataTParam->mutex);
        }

    }
#else

    while (true)
    {
        if (dataTParam->sockQ.empty())
        {
            usleep(10000);
            continue;
        }

        while (dataTParam->sockQ.empty() == false)
        {
            int sock = dataTParam->sockQ.front();
            dataTParam->sockQ.pop_front();

            if (sock < 0)
            {
                LOG_INFO("%d thread receive quit signal", threadIndex);
                return ;
            }

            sendData(sock);
        }

    }
#endif

}

void* Net::SendThread(void *arg)
{
    DataThreadInputParam *dataParam = static_cast<DataThreadInputParam *>(arg);

    Net* net = dataParam->netInstance;
    int  threadIndex = dataParam->threadIndex;

    delete dataParam;

    net->sending(threadIndex);
    LOG_INFO("%d thread exit", threadIndex);

    pthread_exit(0);
}

void Net::recvData(int sock)
{
    LOG_TRACE("enter");

    // Read from socket
    ConnMgr* cm = &getConnMgr();
    Conn *conn = NULL;

    /**
     * @@@ testing code
     */
    //cm->lock();
    MUTEX_LOCK(&cm->mapMutex);
    conn = cm->find(sock);
    MUTEX_UNLOCK(&cm->mapMutex);
    //cm->unlock();
    if (NULL == conn)
    {
        LOG_ERROR("No connection with socket %d", sock);
        //delRecvSelector(sock);
        return;
    }

    Conn::status_t crc = Conn::CONN_ERR_UNAVAIL;
    try{
        crc = conn->recvProgress(true);
    }catch(...)
    {
        LOG_ERROR("Occur exception");
        crc = Conn::CONN_ERR_BROKEN;
    }

    LOG_TRACE("After recvProgress");
    if (crc == Conn::SUCCESS)
    {
        conn->release();
        LOG_TRACE("exit");
        return;
    }
    else if (crc == Conn::CONN_READY)
    {
        // There is more data on this connection.

        // old logic is add the socket/connection to recv thread's readyConns

        //here just add it to epoll

        conn->release();

        addToRecvSelector(sock);

        LOG_TRACE("exit");
        return;
    }
    else if (crc == Conn::CONN_ERR_BROKEN)
    {
        LOG_DEBUG("removing broken conn");

        conn->release();
        removeConn(sock);


        LOG_TRACE("exit");
        return;
    }
    else
    {
        // other error
        // CONN_ERR_UNAVAIL
        conn->release();
        LOG_ERROR("recvProgress error: %d", crc);

        return;
    }
}

void Net::recving(int threadIndex)
{
    LOG_INFO("%d recving data thread has been started", threadIndex);

    DataThreadParam * dataTParam = recvDataThreads[threadIndex];

#if INFORM_WITH_SIGNAL
    MUTEX_LOCK(&dataTParam->mutex);

    while (true)
    {
        COND_WAIT(&dataTParam->cond, &dataTParam->mutex);
        if (dataTParam->sockQ.empty())
        {
            continue;
        }

        while (dataTParam->sockQ.empty() == false)
        {
            int sock = dataTParam->sockQ.front();
            dataTParam->sockQ.pop_front();
            MUTEX_UNLOCK(&dataTParam->mutex);

            if (sock < 0)
            {
                LOG_INFO("recv %d thread receive quit signal", threadIndex);
                return ;
            }

            recvData(sock);
            MUTEX_LOCK(&dataTParam->mutex);
        }

    }
#else

    while (true)
    {
        if (dataTParam->sockQ.empty())
        {
            usleep(10000);
            continue;
        }

        while (dataTParam->sockQ.empty() == false)
        {
            int sock = dataTParam->sockQ.front();
            dataTParam->sockQ.pop_front();

            if (sock < 0)
            {
                LOG_INFO(" recv %d thread receive quit signal", threadIndex);
                return ;
            }

            recvData(sock);
        }

    }
#endif
}

void* Net::RecvThread(void *arg)
{
    DataThreadInputParam *dataParam = static_cast<DataThreadInputParam *>(arg);

    Net* net = dataParam->netInstance;
    int threadIndex = dataParam->threadIndex;

    delete dataParam;

    net->recving(threadIndex);

    LOG_INFO("%d thread exit", threadIndex);
    pthread_exit(0);
}

void Net::prepareRecv(int sock)
{
    LOG_TRACE("Enter");

    int threadIndex = sock % recvDataThreads.size();
    DataThreadParam *dataParam = recvDataThreads[threadIndex];

    MUTEX_LOCK(&dataParam->mutex);
    dataParam->sockQ.push_back(sock);
#if INFORM_WITH_SIGNAL
    COND_SIGNAL(&dataParam->cond);
#endif
    MUTEX_UNLOCK(&dataParam->mutex);

    LOG_TRACE("Exit");
}

void*
Net::RecvEPollThread(void *arg)
{
    Net* net = static_cast<Net*>(arg);

    int epfd = net->recvEpfd;
    int notifyFd = net->recvPfd[0];

    ConnMgr* cm = &net->connMgr;

    int listenSock = Sock::DISCONNECTED;

    // The following parameters are of interest only to servers
    if (NetServer *netsrv = dynamic_cast<NetServer*>(net))
    {
        listenSock = netsrv->getListenSock();
    }

    int nfds, i;
    Conn *conn;
    bool exitCmd;

    LOG_INFO("Start net receive epoll thread");

    ASSERT((epfd && (notifyFd >= 0) && cm), "incorrect arguments");

    struct epoll_event* events = new struct epoll_event[MAX_EPOLL_EVENTS];
    while (true)
    {
        nfds = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);
        exitCmd = false;

        for (i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if (fd == notifyFd)
            {
                char info[THREAD_INFO_LEN];
                int nr;
                Sock::readBlocking(notifyFd, info, THREAD_INFO_LEN, nr);

                LOG_TRACE("received signal: %s", info);

                // Record exit command but process all events before exiting
                if (strcmp(info, THREAD_INFO_EXIT) == 0)
                    exitCmd = true;
                continue;
            }

            // Get Conn from ConnMgr
            if (events[i].events & (EPOLLHUP | EPOLLERR))
            {
                LOG_INFO("RecvThread - poll hung up or poll error"
                " event:%d, socket:%d", events[i].events, fd);
                if (fd == listenSock)
                {
                    // Check if fd with error is listenSock. If yes, need to
                    // re-establish listenSock
                }
                else
                {
                    LOG_INFO("broken socket");
                    /**
                     * @@@ testing code
                     */
                    MUTEX_LOCK(&cm->mapMutex);
                    //cm->lock();
                    conn = cm->find(fd);
                    //cm->unlock();
                    MUTEX_UNLOCK(&cm->mapMutex);
                    if (conn)
                    {
                        LOG_INFO("conn found - removing");
                        net->removeConn(fd);
                        conn->release();
                    }
                    else
                    {

                        LOG_INFO("conn has already been removed");
                        net->delRecvSelector(fd);
                    }
                }
                continue;
            }

            if (fd == listenSock)
            {
                net->acceptConns();
                continue;
            }

            net->prepareRecv(fd);
        }

        // Check whether the thread has received exit notification
        if (exitCmd)
            break;
    }

    delete[] events;
    LOG_INFO("exit receive thread");

    pthread_exit(0);
}

Conn* Net::getConn(EndPoint& ep, bool serverSide, int sock)
{
    LOG_TRACE("enter");

    /**
     * @@@ testing code
     */
    //connMgr.lock();
    MUTEX_LOCK(&connMgr.mapMutex);
    Conn* conn = NULL;

    if (sock != -1)
    {
        conn = connMgr.find(sock);
    }
    else
    {
        conn = connMgr.find(ep);
    }

    if (conn)
        LOG_DEBUG("getConn - socket of conn:%d", conn->getSocket());

    if (!conn && !serverSide)
    {
        LOG_INFO("make conn to: %s:%d", ep.getHostName(), ep.getPort());

        conn = new Conn();
        if (!conn)
        {
            //connMgr.unlock();
            MUTEX_UNLOCK(&connMgr.mapMutex);
            LOG_ERROR("Failed to new conn");
            throw NetEx(NET_ERR_CONN_NOTFOUND, "Failed to new conn");

        }

        int sock = Sock::DISCONNECTED; // initialize to disconnected state
        Conn::status_t crc = conn->connect(ep, sock);
        if (crc == Conn::CONN_ERR_CONNECT)
        {
            //connMgr.unlock();
            MUTEX_UNLOCK(&connMgr.mapMutex);
            conn->cleanup(Conn::ON_ERROR);

            std::string hostPort;
            ep.toHostPortStr(hostPort);

            std::string msg = "can't connect to server: " + hostPort;
            throw NetEx(NET_ERR_CONNECT, msg);
        }

        conn->setState(crc);

        int connStatus = conn->connCallback(Conn::ON_CONNECT);
        if (connStatus)
        {
            MUTEX_UNLOCK(&connMgr.mapMutex);
            conn->cleanup(Conn::ON_ERROR);
            //connMgr.unlock();

            std::string msg = "conn callback failed";
            throw NetEx(NET_ERR_CONNECT, msg);
        }

        // Insert new connection in ConnMgr map. This has to be done before
        // adding the socket to the send and receive selectors
        connMgr.insert(ep, sock, conn);
        conn->setPeerEp(ep);


        // Add the socket to the send selector
        // We are safe to add sock here even the connection is finished before.
        // During adding, the status of the sock will be checked, if it is
        // ready when being added, it will be put on a ready list.
        // Next time when we call epoll_wait(), this socket will be returned
        // even if we are using the edge-triggered mode.
        Net::status_t rc = addToSendSelector(sock);
        if (rc != SUCCESS)
        {
            MUTEX_UNLOCK(&connMgr.mapMutex);
            //conn->cleanup will be done in connMgr.remove
            removeConn(sock);
            //connMgr.unlock();

            throw NetEx(NET_ERR_EPOLL, "cannot add socket to send selector");
        }

        // Add the socket to the recv selector
        rc = addToRecvSelector(sock);
        if (rc != SUCCESS)
        {
            //conn->cleanup will be done in connMgr.remove
            MUTEX_UNLOCK(&connMgr.mapMutex);
            removeConn(sock);

            //connMgr.unlock();
            throw NetEx(NET_ERR_EPOLL, "cannot add socket to recv selector");
        }

        conn->acquire();
    }
    else if (!conn && serverSide)
    {
        connMgr.list();
        //connMgr.unlock();
        MUTEX_UNLOCK(&connMgr.mapMutex);
        LOG_WARN("client at %s:%d has already closed the connection",
                ep.getHostName(), ep.getPort());


        throw NetEx(NET_ERR_CONN_NOTFOUND, "conn already removed");
    }

    //connMgr.unlock();
    MUTEX_UNLOCK(&connMgr.mapMutex);

    LOG_TRACE("exit");

    return conn;
}

void Net::addConn(Conn* conn, EndPoint& ep, int sock)
{
    LOG_TRACE("enter: adding conn to: %s:%d", ep.getHostName(), ep.getPort());

    // Conn is allocated by caller
    /**
     * @@@ testing code
     */
    //connMgr.lock();
    MUTEX_LOCK(&connMgr.mapMutex);
    Conn* tconn = connMgr.find(sock);
    if (tconn)
    {
        tconn->release();
        //connMgr.unlock();
        MUTEX_UNLOCK(&connMgr.mapMutex);

        // Cleanup the connection before return
        conn->cleanup(Conn::ON_ERROR);

        throw NetEx(NET_ERR_CONN_EXISTS, "Connection exists");
    }
    conn->setup(sock);

    // Set connection status
    conn->setState(Conn::CONN_READY);
    conn->setPeerEp(ep);

    // Insert connection in ConnMgr map and add connection socket
    // to send and receive selectors. It is important to keep the order
    // of these operations: inserting the connection should precede addition
    // to the selectors.
    connMgr.insert(ep, sock, conn);
    status_t rc = addToRecvSelector(sock);
    if (rc != SUCCESS)
    {
        MUTEX_UNLOCK(&connMgr.mapMutex);
        //connMgr.unlock();
        removeConn(sock);
        throw NetEx(NET_ERR_EPOLL, "Cannot add socket to receive selector");
    }
    rc = addToSendSelector(sock);
    if (rc != SUCCESS)
    {
        MUTEX_UNLOCK(&connMgr.mapMutex);
        removeConn(sock);
        //connMgr.unlock();
        throw NetEx(NET_ERR_EPOLL, "Cannot add socket to send selector");
    }
    //connMgr.unlock();
    MUTEX_UNLOCK(&connMgr.mapMutex);

    LOG_TRACE("exit");
}

Net::status_t Net::delConn(EndPoint& ep)
{
    LOG_TRACE("enter");
    /**
     * @@@ TESTING CODE
     */
    MUTEX_LOCK(&connMgr.mapMutex);
    //connMgr.lock();
    Conn *conn = connMgr.find(ep);
    MUTEX_UNLOCK(&connMgr.mapMutex);
    //connMgr.unlock();
    if (!conn)
    {
        return NET_ERR_CONN_NOTFOUND;
    }
    // remove from ConnMgr and disconnect
    conn->release();
    removeConn(conn->getSocket());

    LOG_TRACE("exit");

    return SUCCESS;
}

bool Net::initialized()
{
    bool flag;
    MUTEX_LOCK(&netMutex);
    flag = initFlag;
    MUTEX_UNLOCK(&netMutex);

    return flag;
}

bool Net::finalized()
{
    bool flag;
    MUTEX_LOCK(&netMutex);
    flag = shutdownFlag;
    MUTEX_UNLOCK(&netMutex);
    return flag;
}

Net::status_t Net::addToSendSelector(int sock)
{
    Sock::status_t rc = Sock::addToSelector(sock, Sock::DIR_OUT, sendEpfd);
    if (rc != Sock::SUCCESS)
    {
        char errbuf[ERR_BUF_SIZE], *errptr;
        errptr = strerror_r(errno, errbuf, ERR_BUF_SIZE);
        LOG_ERROR("Failed to add sock to send epoll fd: %s", errptr);

        return NET_ERR_EPOLL;
    }

    return SUCCESS;
}

void Net::delSendSelector(int sock)
{
    Sock::status_t rc = Sock::rmFromSelector(sock, sendEpfd);
    if (rc != Sock::SUCCESS)
    {
        LOG_ERROR("Failed to close %d send epoll", sendEpfd);
    }
    else
    {
        LOG_INFO("Delete %d in send epoll ", sock);
    }
}

Net::status_t Net::addToRecvSelector(int sock)
{
    Sock::status_t rc = Sock::addToSelector(sock, Sock::DIR_IN, recvEpfd);
    if (rc != Sock::SUCCESS)
    {
        char errbuf[ERR_BUF_SIZE], *errptr;
        errptr = strerror_r(errno, errbuf, ERR_BUF_SIZE);
        LOG_ERROR("Failed to add sock to recv epoll fd: %s", errptr);

        return NET_ERR_EPOLL;
    }

    return SUCCESS;
}

void Net::delRecvSelector(int sock)
{
    Sock::status_t rc = Sock::rmFromSelector(sock, recvEpfd);
    if (rc != Sock::SUCCESS)
    {
        LOG_ERROR("Failed to close %d recv epoll", sock);
    }
    else
    {
        LOG_INFO("Delete %d in recv epoll ", sock);
    }
}

Net::status_t Net::removeConn(int sock)
{
    //connMgr.lock();
    /**
     * @@@ TESTING CODE
     */
    MUTEX_LOCK(&connMgr.mapMutex);
    ConnMgr::status_t rc = connMgr.remove(sock);
    //connMgr.unlock();
    MUTEX_UNLOCK(&connMgr.mapMutex);
    return (rc == ConnMgr::SUCCESS) ? SUCCESS : NET_ERR_CONN_NOTFOUND;
}

void Net::acceptConns()
{
    LOG_WARN("Shouldn't receive accept connection event");

    return;
}

size_t Net::removeInactive()
{
    /**
     * @@@ TESTING CODE
     */
    //connMgr.lock();
    MUTEX_LOCK(&connMgr.mapMutex);
    size_t ret = connMgr.removeInactive();
    MUTEX_UNLOCK(&connMgr.mapMutex);
    //connMgr.unlock();

    return ret;
}

ConnMgr& Net::getConnMgr()
{
    return connMgr;
}

Stage *Net::getCommStage()
{
    return mCommStage;
}
