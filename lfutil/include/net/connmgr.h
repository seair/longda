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

#ifndef _CONNMGR_HXX_
#define _CONNMGR_HXX_

#include <map>
#include <set>

#include "mm/lmpool.h"
#include "os/mutex.h"
#include "net/conn.h"

//! Connection manager
/**
 * @author Longda
 * @date   5/05/07
 *
 * ConnMgr maintains maps between Conn objects and end point descriptors
 * or socket file descriptors.
 *
 */
class ConnMgr
{
public:
    //! Enumeration for return status codes
    typedef enum
    {
        SUCCESS = 0,        //!< successful operation
        ERR_NOT_FOUND,      //!< connection not found
        ERR_AINSERTED,      //!< connection is already inserted
        ERR_LOCK_BUSY,      //!< lock is busy, taken by another thread
        ERR_LOCK_AOWNED,    //!< attempt to lock an already owned lock
        ERR_LOCK_NOTOWNED,  //!< attempt to unlock without a prior lock
        ERR_LOCK            //!< some other locking error
    } status_t;

public:
    //! Constructor
    /**
     * Creates a connection manager and initializes internal lock and 
     * map containers
     */
    ConnMgr();

    //! Destructor
    /**
     * Cleans up map containers and destroys map lock
     */
    ~ConnMgr();

    //! Find a connection by socket
    /**
     * Finds a connection by socket. 
     * 
     * @param[in]   sock    socket to lookup connection by
     * @return connection found or 0 if no connection for this socket is found
     */
    Conn* find(const int& sock);

    //! Find a connection by an end point
    /**
     * Find a connection object in the map by an end point.
     * @param[in]   ep  end point to look up the conncetion by
     * @return  connection or 0 if no connection for this end point is found
     */
    Conn* find(EndPoint& ep);

    //! Insert aconnection
    /**
     * Inserts a new connection in the internal maps. Attempting to insert
     * the same connection twice is erroneous. 
     * 
     * @param[in]   ep      end point
     * @param[in]   sock    socket
     * @param[in]   conn    connection
     * return status (SUCCESS, ERR_AINSERTED) 
     */
    ConnMgr::status_t insert(EndPoint& ep, const int sock, Conn* conn);

    //! Remove a connection by socket
    /**
     * Removes a connection from the internal maps using a socket as key
     * An error is returned if the connection is not found.
     *
     * @param[in]   sock    socket to be used as key
     * @return  status (SUCCESS, ERR_NOT_FOUND)
     */
    ConnMgr::status_t remove(const int& sock);

    //! Removes a connection by connection object pointer
    /**
     * Removes a connection from the ConnMgr's maps by the connection
     * pointer. This is the slowest of all three methods from the remove
     * family becasuse it does two reverse linear searches.
     * 
     * @param[in]   conn    connection to be removed
     * @return  status (SUCCESS, ERR_NOT_FOUND)
     */
    ConnMgr::status_t remove(Conn* conn);

    //! Remove connections not active for the longest time
    /**
     * @return the number of removed connections
     */
    size_t removeInactive();

    //! Lock internal maps mutex
    /**
     * Internal maps must be locked before a new connection is inserted,
     * an old one is removed, or a look up for a connection is performed. If
     * the calling thread already owns the lock, ERR_LOCK_AOWNED is returned.
     * 
     * @return status (SUCCESS, ERR_LOCK_AOWNED)
     * 
     * @pre the internal mutex is unlocked
     * @post the internal mutex is locked
     */
    ConnMgr::status_t lock();

    //! Try to lock internal maps mutex
    /**
     * Tries to lock the internal mutex. If the lock is busy, returns
     * ERR_LOCK_BUSY. If lock is acquired, returns SUCCESS.
     * 
     * @return status (SUCCESS, ERR_LOCK_AOWNED, ERR_LOCK_BUSY, ERR_LOCK)
     */
    ConnMgr::status_t trylock();

    //! Unlock internal maps mutex
    /**
     * Unlocks maps mutex.  If the calling thread does not own the mutex,
     * ERR_LOCK_NOTOWNED status code is returned. 
     * 
     * @return status (SUCCESS, ERR_LOCK_NOTOWNED, ERR_LOCK)
     */
    ConnMgr::status_t unlock();

    //! Lists all members of the connection map
    /**
     * A helper method to list the content of the connection map
     */
    void list();

    /**
     * create Conn memory pool
     */
    int initConnPool(int initSzie);

    /**
     * alloc Conn from memory pool
     */
    Conn* allocConn();

    void freeConn(Conn * conn);

private:
    typedef std::map<std::string, std::set<int> > EpSockMap;
    std::map<int, Conn*> sockConnMap;           //!< map between socket and Conn


    EpSockMap epSockMap; //!< map between EndPoint and scoket
    pthread_mutex_t mapMutex;                          //!< mutex for both maps

    CLmpool<Conn > connPool;

    //! Copy constructor
    /**
     * Private in order to prevent accidentla use of the default one
     */
    ConnMgr(const ConnMgr& cm);

    //! Assignment operator
    /**
     * Private in order to prevent accidentla use of the default one
     */
    ConnMgr& operator=(const ConnMgr& cm);


};

#endif // _CONNMGR_HXX_
