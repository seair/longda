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

#include <unistd.h>
#include <iostream>
#include <map>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "trace/log.h"
#include "os/mutex.h"
#include "lang/lstring.h"

#include "net/connmgr.h"

//! Implementation of ConnMgr
/**
 * @file
 * @author Longda
 * @date   5/12/07
 *
 * Connection manager maintains connection objects. When a new connection
 * is created, ConnMgr enters it in its map. When a connection is destroyed,
 * the connection is removed from the map. Lookup methods for finding 
 * connection by sock file decriptors and end point specifiers are provided.
 *
 * ConnMgr methods for inserting, removing, and looking up connections
 * are not internally protcted by a mutex lock. The caller is responsible
 * for locking/unlocking the ConnMgr internal lock by invoking the
 * lock(), unlock(), and trylock() methods.
 */

// number of inactive connections removed each time run out of fd
static size_t NUM_REMOVE_INACTIVE = 64;

ConnMgr::ConnMgr() :
        sockConnMap(), epSockMap(), connPool()
{
    LOG_TRACE("enter");
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    pthread_mutex_init(&mapMutex, &attr);
    LOG_TRACE("exit");
}

ConnMgr::~ConnMgr()
{
    LOG_TRACE("enter");

    // Cleanup maps
    MUTEX_LOCK(&mapMutex);
    epSockMap.clear();

    std::map<int, Conn*>::iterator ci;
    for (ci = sockConnMap.begin(); ci != sockConnMap.end(); ++ci)
    {
        Conn* conn = ci->second;
        conn->cleanup(Conn::ON_CLEANUP);
    }
    sockConnMap.clear();

    MUTEX_UNLOCK(&mapMutex);
    pthread_mutex_destroy(&mapMutex);

    LOG_TRACE("exit");
}

Conn*
ConnMgr::find(const int& sock)
{
    Conn *conn = 0;
    std::map<int, Conn*>::iterator p = sockConnMap.find(sock);
    if (p != sockConnMap.end())
    {
        conn = p->second;
        /**
         * FIXME, why here increase the counter?
         */
        conn->acquire();
    }
    return conn;
}

Conn*
ConnMgr::find(EndPoint& ep)
{
    std::string keyStr;

    ep.toHostPortStr(keyStr);
    EpSockMap::iterator p = epSockMap.find(keyStr);
    if (p == epSockMap.end())
        return NULL;

    int socket = *(p->second.begin());
    return find(socket);
}

ConnMgr::status_t ConnMgr::insert(EndPoint& ep, const int sock,
        Conn* conn)
{
    LOG_TRACE( "%s", "enter");

    // Make sure there is no old connection for the same socket
    std::map<int, Conn*>::iterator c = sockConnMap.find(sock);
    if (c != sockConnMap.end())
    {
        LOG_ERROR( "%s", "connection already inserted");
        return ERR_AINSERTED;
    }
    sockConnMap.insert(std::pair<int, Conn*>(sock, conn));

    // Make a string out of ep.host and ep.port
    std::string keyStr;
    ep.toHostPortStr(keyStr);

    // Verify whether there is a leftover connection for this key 
    EpSockMap::iterator p = epSockMap.find(keyStr);
    if (p == epSockMap.end())
    {
        std::set<int> emptySet;

        epSockMap.insert(
                std::pair<std::string, std::set<int> >(keyStr, emptySet));
    }

    p = epSockMap.find(keyStr);
    if (p != epSockMap.end())
    {
        std::set<int> &socketSet = (p->second);

        socketSet.insert(sock);
    }

    LOG_INFO( "inserted connection  %s:%d to connMgr", keyStr.c_str(), sock);

    LOG_TRACE("Exit");
    return SUCCESS;
}


ConnMgr::status_t ConnMgr::remove(const int& sock)
{
    LOG_TRACE("enter");

    status_t rc = SUCCESS;

    /**
     * delete it from EpSockMap
     */
    for (EpSockMap::iterator si = epSockMap.begin(); si != epSockMap.end();
            ++si)
    {
        std::set<int> &sockSet = (si->second);
        if (sockSet.find(sock) != sockSet.end())
        {
            sockSet.erase(sock);
            if (sockSet.empty())
            {
                epSockMap.erase(si);
            }
            break;
        }
    }

    // This method is called only when there is an erroneous condition. 
    // Normal connection cleanup is done in ConnMgr's destructor
    std::map<int, Conn*>::iterator ci = sockConnMap.find(sock);
    if (ci != sockConnMap.end())
    {
        Conn* conn = ci->second;
        conn->cleanup(Conn::ON_ERROR);
        sockConnMap.erase(ci);
    }
    else
        rc = ERR_NOT_FOUND;

    LOG_TRACE("exit");

    return rc;
}

ConnMgr::status_t ConnMgr::remove(Conn* conn)
{
    LOG_TRACE("enter");

    int sock = -1;
    std::map<int, Conn*>::iterator ci;
    for (ci = sockConnMap.begin(); ci != sockConnMap.end(); ++ci)
    {
        if (ci->second == conn)
            sock = ci->first;
    }

    status_t rv = ERR_NOT_FOUND;
    if (sock > 0)
        rv = remove(sock);

    LOG_TRACE("exit");

    return rv;
}

size_t ConnMgr::removeInactive()
{
    // activity sn -- socket map
    std::map<u64_t, int> sortedMap;

    std::map<int, Conn*>::iterator ci;
    for (ci = sockConnMap.begin(); ci != sockConnMap.end(); ++ci)
    {
        // skip connections in use
        if (!ci->second->isIdle())
            continue;

        if (sortedMap.size() < NUM_REMOVE_INACTIVE)
        {
            sortedMap.insert(std::make_pair(ci->second->getActSn(), ci->first));
        }
        else
        {
            // have collected NUM_REMOVE_INACTIVE connections in map,
            // check if should replace one in map with this
            std::map<u64_t, int>::iterator last = --sortedMap.end();
            if (ci->second->getActSn() < last->first)
            {
                sortedMap.erase(last);
                sortedMap.insert(
                        std::make_pair(ci->second->getActSn(), ci->first));
            }
        }
    }

    size_t removed = 0;

    // remove those connections
    std::map<u64_t, int>::iterator si;
    for (si = sortedMap.begin(); si != sortedMap.end(); ++si)
    {
        if (remove(si->second) == SUCCESS)
            removed++;
    }

    return removed;
}

ConnMgr::status_t ConnMgr::lock()
{
    int rc = MUTEX_LOCK(&mapMutex);
    if (rc == 0)
        return SUCCESS;
    if (rc == EDEADLK)
        return ERR_LOCK_AOWNED;
    return ERR_LOCK;
}

ConnMgr::status_t ConnMgr::trylock()
{
    int rc = MUTEX_TRYLOCK(&mapMutex);
    if (rc == 0)
        return SUCCESS;
    if (rc == EDEADLK)
        return ERR_LOCK_AOWNED;
    if (rc == EBUSY)
        return ERR_LOCK_BUSY;
    return ERR_LOCK;
}

ConnMgr::status_t ConnMgr::unlock()
{
    int rc = MUTEX_UNLOCK(&mapMutex);
    if (rc == 0)
        return SUCCESS;
    if (rc == EPERM)
        return ERR_LOCK_NOTOWNED;
    return ERR_LOCK;
}

void ConnMgr::list()
{
    EpSockMap::iterator p;
    for (p = epSockMap.begin(); p != epSockMap.end(); p++)
    {
        std::string   hostPort = p->first;

        std::cout << "ConnMgr entry: " << hostPort;

        std::set<int> &sockSet = p->second;
        std::set<int>::iterator sockIt;
        for(sockIt = sockSet.begin(); sockIt != sockSet.end(); sockIt++)
        {
            Conn *conn = find(*sockIt);
            if (conn)
            {
                std::cout <<",sock:" <<  *sockIt << ",conn:" << conn;
                conn->release();
            }
            else
            {
                std::cout <<",sock:" <<  *sockIt << ",conn:NULL";
            }

        }
        std::cout << std::endl;
    }
}

int ConnMgr::initConnPool(int initSize)
{
    return connPool.init(initSize);
}

Conn* ConnMgr::allocConn()
{
    return connPool.get();
}

void ConnMgr::freeConn(Conn * conn)
{
    conn->cleanup(Conn::ON_CLEANUP);

    connPool.put(conn);
}

