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

//! Implementation of Net
/**
 * @file
 * @author Longda
 * @date   5/05/07
 * 
 */

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

int Net::startThreads()
{
    int rc;
    rc = pthread_create(&recvThreadId, NULL, RecvThread, this);
    if (rc != 0)
    {
        LOG_ERROR("create recv thread failed, %d:%s", rc, strerror(rc));
        return rc;
    }
    rc = pthread_create(&sendThreadId, NULL, SendThread, this);
    if (rc != 0)
    {
        LOG_ERROR("create send thread failed, %d:%s", rc, strerror(rc));
        return rc;
    }

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

void*
Net::SendThread(void *arg)
{
    Net* net = static_cast<Net*>(arg);

    int epfd = net->sendEpfd;
    int notifyFd = net->sendPfd[0];
    ConnMgr* cm = &net->connMgr;

    int nfds, fd, i;
    bool exitCmd;

    struct epoll_event* events = new struct epoll_event[MAX_EPOLL_EVENTS];

    LOG_INFO("Start network send thread");

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

            cm->lock();
            Conn* conn = cm->find(fd);
            if (conn == NULL)
            {
                LOG_INFO("conn has been removed");

                if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == 0)
                {
                    LOG_INFO("sock still in send epfd; closing sock");
                }
                cm->unlock();
                continue;
            }

            // Check for error events
            if (events[i].events & (EPOLLHUP | EPOLLERR))
            {
                // Error on socket - remove the corresponding conn
                LOG_ERROR("detected broken inet socket %s:%d - removing conn",
                        conn->getPeerEp().getHostName(), conn->getPeerEp().getPort());

                net->removeConn(fd);
                conn->release();
                cm->unlock();
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
                    LOG_ERROR(
                            "detect connect error, removing conn, socket error:%d",
                            error);
                    net->removeConn(fd);
                    conn->release();
                    cm->unlock();
                    continue;
                }

                LOG_INFO("connecting is finished OK...");
                conn->setState(Conn::CONN_READY);
            }

            cm->unlock();

            CommSendEvent *event = new CommSendEvent(conn);
            net->getCommStage()->addEvent(event);
        }

        // Check whether the thread has received exit notification
        if (exitCmd)
            break;
    }

    delete[] events;

    LOG_INFO("exit send thread");

    pthread_exit(0);
}

void*
Net::RecvThread(void *arg)
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

    LOG_INFO("Start net receive thread");

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
                    cm->lock();
                    conn = cm->find(fd);
                    if (conn)
                    {
                        LOG_INFO("conn found - removing");
                        net->removeConn(fd);
                        conn->release();
                        /**
                         * @@@ FIXME
                         * add operation to delete epoll listen?
                         * check whether close(sock) will trigger later or not?
                         */
                    }
                    else
                    {

                        LOG_INFO("conn has already been removed");
                        // The connection was not found - it must have
                        // been removed before. Try to delete the socket
                        // from the receive epoll fd.
                        if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == 0)
                        {
                            LOG_DEBUG("sock still in epfd; closing it");
                        }
                    }
                    cm->unlock();
                }
                continue;
            }

            if (fd == listenSock)
            {
                net->acceptConns();
                continue;
            }

            CommRecvEvent *event = new CommRecvEvent(fd);
            net->getCommStage()->addEvent(event);
        }

        // Check whether the thread has received exit notification
        if (exitCmd)
            break;
    }

    delete[] events;
    LOG_INFO("exit receive thread");

    pthread_exit(0);
}

Conn* Net::getConn(EndPoint& ep, bool serverSide)
{
    LOG_TRACE("enter");

    connMgr.lock();
    Conn* conn = connMgr.find(ep);
    if (conn)
        LOG_DEBUG("getConn - socket of conn:%d", conn->getSocket());

    if (!conn && !serverSide)
    {
        LOG_INFO("make conn to: %s:%d", ep.getHostName(), ep.getPort());

        conn = new Conn();
        if (!conn)
        {
            connMgr.unlock();
            LOG_ERROR("Failed to new conn");
            throw NetEx(NET_ERR_CONN_NOTFOUND, "Failed to new conn");

        }

        int sock = Sock::DISCONNECTED; // initialize to disconnected state
        Conn::status_t crc = conn->connect(ep, sock);
        if (crc == Conn::CONN_ERR_CONNECT)
        {
            connMgr.unlock();
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
            conn->cleanup(Conn::ON_ERROR);
            connMgr.unlock();
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
            //conn->cleanup will be done in connMgr->remove
            removeConn(sock);
            connMgr.unlock();
            throw NetEx(NET_ERR_EPOLL, "cannot add socket to send selector");
        }

        // Add the socket to the recv selector
        rc = addToRecvSelector(sock);
        if (rc != SUCCESS)
        {
            //conn->cleanup will be done in connMgr->remove
            removeConn(sock);
            connMgr.unlock();
            throw NetEx(NET_ERR_EPOLL, "cannot add socket to recv selector");
        }
        conn->acquire();
    }
    else if (!conn && serverSide)
    {
        connMgr.list();
        connMgr.unlock();
        LOG_WARN("client at %s:%d has already closed the connection",
                ep.getHostName(), ep.getPort());


        throw NetEx(NET_ERR_CONN_NOTFOUND, "conn already removed");
    }

    connMgr.unlock();

    LOG_TRACE("exit");

    return conn;
}

void Net::addConn(Conn* conn, EndPoint& ep, int sock)
{
    LOG_TRACE("enter: adding conn to: %s:%d", ep.getHostName(), ep.getPort());

    // Conn is allocated by caller
    connMgr.lock();
    Conn* tconn = connMgr.find(sock);
    if (tconn)
    {
        tconn->release();
        connMgr.unlock();

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
        removeConn(sock);
        connMgr.unlock();
        throw NetEx(NET_ERR_EPOLL, "Cannot add socket to receive selector");
    }
    rc = addToSendSelector(sock);
    if (rc != SUCCESS)
    {
        removeConn(sock);
        connMgr.unlock();
        throw NetEx(NET_ERR_EPOLL, "Cannot add socket to send selector");
    }
    connMgr.unlock();

    LOG_TRACE("exit");
}

Net::status_t Net::delConn(EndPoint& ep)
{
    LOG_TRACE("enter");
    connMgr.lock();
    Conn *conn = connMgr.find(ep);
    if (!conn)
    {
        connMgr.unlock();
        return NET_ERR_CONN_NOTFOUND;
    }
    // remove from ConnMgr and disconnect
    removeConn(conn);
    conn->release();
    connMgr.unlock();
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
    int rc = epoll_ctl(sendEpfd, EPOLL_CTL_DEL, sock, NULL);
    if (rc)
    {
        LOG_ERROR("Failed to close %d epoll listen", sock);
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
    int rc = epoll_ctl(recvEpfd, EPOLL_CTL_DEL, sock, NULL);
    if (rc)
    {
        LOG_ERROR("Failed to close %d epoll listen", sock);
    }
}

// Has to be called within the context of a locked connMgr
Net::status_t Net::removeConn(Conn* conn)
{
    if (!conn)
        return NET_ERR_CONN_NOTFOUND;

    ConnMgr::status_t rc = connMgr.remove(conn);
    return (rc == ConnMgr::SUCCESS) ? SUCCESS : NET_ERR_CONN_NOTFOUND;
}

Net::status_t Net::removeConn(int sock)
{
    ConnMgr::status_t rc = connMgr.remove(sock);
    return (rc == ConnMgr::SUCCESS) ? SUCCESS : NET_ERR_CONN_NOTFOUND;
}

void Net::acceptConns()
{
    LOG_WARN("Shouldn't receive accept connection event");

    return;
}

size_t Net::removeInactive()
{
    connMgr.lock();
    size_t ret = connMgr.removeInactive();
    connMgr.unlock();

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
