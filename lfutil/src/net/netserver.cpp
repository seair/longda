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
 * netserver.cpp
 *
 *  Created on: Apr 26, 2012
 *      Author: Longda Feng
 */

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

#include "net/netex.h"
#include "net/netserver.h"



NetServer::NetServer(Stage *commStage) :
        Net(commStage),
        mPort(0),
        mListenSock(Sock::DISCONNECTED)
{
}

NetServer::NetServer(Stage *commStage, unsigned short Port) :
        Net(commStage),
        mPort(Port),
        mListenSock(Sock::DISCONNECTED)
{
}

NetServer::~NetServer()
{
}

void NetServer::setPort(unsigned short port)
{
    this->mPort = port;
}

unsigned short NetServer::getPort() const
{
    return mPort;
}

int NetServer::getListenSock() const
{
    return mListenSock;
}

int NetServer::setup()
{
    int rc;

    LOG_TRACE("enter");
    if (mPort == 0)
    {     // NetServer must have a mPort to listen to
        LOG_ERROR("No mPort has been set for server mode");
        return -EINVAL;
    }

    MUTEX_LOCK(&netMutex);
    if (initFlag)
    {
        MUTEX_UNLOCK(&netMutex);

        LOG_WARN("Has already init");
        return 0;
    }
    rc = Net::setupSelectors();
    if (rc != 0)
    {
        MUTEX_UNLOCK(&netMutex);

        LOG_ERROR("setup of semd/recv selectors failed, %d", rc);
        return rc;
    }

    // Special for NetServer
    rc = Sock::setupListener(mPort, mListenSock, Conn::getLsnSndBufSz(),
            Conn::getLsnRcvBufSz());
    if (rc != Sock::SUCCESS)
    {
        MUTEX_UNLOCK(&netMutex);

        LOG_ERROR("could not setup inet listener socket, %d", rc);
        return rc;
    }

    Sock::setNonBlocking(mListenSock);

    rc = Sock::addToSelector(mListenSock, Sock::DIR_IN, recvEpfd);
    if (rc != Sock::SUCCESS)
    {
        MUTEX_UNLOCK(&netMutex);

        LOG_ERROR("failed to add inet listener socket to epfd, %d", rc);
        return rc;
    }

    rc = Net::startThreads();
    if (rc )
    {
        LOG_ERROR("failed to start threads, %d", rc);

        MUTEX_UNLOCK(&netMutex);

        return rc;
    }
    initFlag = true;


    MUTEX_UNLOCK(&netMutex);
    LOG_INFO("Successfully setup net server");

    return 0;
}

int NetServer::shutdown()
{
    LOG_TRACE("%s", "enter");

    int rc = Net::shutdown();

    if (rc )
    {
        LOG_ERROR("shutdown failed");

        return rc;
    }

    close(mListenSock);

    LOG_INFO("Successfully shutdown net server");

    return 0;
}


void NetServer::acceptConns()
{
    LOG_TRACE("enter");

    struct sockaddr_in c_addr;
    socklen_t clen = sizeof(struct sockaddr_in);

    do {
        int sock = accept(mListenSock, (struct sockaddr *)&c_addr, &clen);
        if(sock < 0) {
            if(errno == EAGAIN) {
                // We have accepted all pending requests - return and
                // go back to epoll_wait to listen for new connection
                break;
            } else if(errno == EMFILE || errno == ENFILE) {
                // Error while accepting this socket.
                LOG_ERROR("accept error: %s", strerror(errno));

                // Continue after close some inactive connections as there may
                // be other connection requests pending on the listener socket
                if (removeInactive() > 0)
                    continue;

                // TODO: restart process if still out of fd?
                return;
            } else {
                // try to accept other connections
                continue;
            }
        }

        Sock::setCloExec(sock);

        // Create a connection object for this new socket
        struct in_addr *p = (struct in_addr *)&c_addr.sin_addr.s_addr;

        std::string remote;
        Sock::netToPrintAddr(p, remote);

        EndPoint ep;
        ep.setHostName(remote.c_str());


        Conn* conn = new Conn();

        // Invoke connection acceptConnCB for the new connection
        int rv = conn->connCallback(Conn::ON_CONNECT);
        if(rv)
        {
            LOG_ERROR("Failed to run conn callback");
            delete conn;
            return ;
        }

        // Add the new connection to the connection map
        try {
            addConn(conn, ep, sock);

        } catch(NetEx ex) {
            // The connection will be deleted in addConn() before an
            // exception is thrown out, do not cleanup it again here
            LOG_ERROR("error adding conn: %s", ex.message.c_str());
        }

        LOG_INFO("new socket accepted: %s", ep.getHostName());
    } while(true);

    LOG_TRACE("exit");
}
