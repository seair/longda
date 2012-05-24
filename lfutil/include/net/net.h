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

#ifndef _NETUTIL_HXX_
#define _NETUTIL_HXX_

#include <pthread.h>
#include <vector>
#include <deque>

#include "seda/stage.h"

#include "net/iovec.h"
#include "net/conn.h"
#include "net/connmgr.h"

#include "comm/message.h"


// Messages to internal threads
#define THREAD_INFO_EXIT    "exit" 

// Types of supported communication services
#define MSG_TYPE_PING       "ping"
#define MSG_TYPE_ECHO       "echo"
#define MSG_TYPE_DATA       "data"
#define MSG_TYPE_RPC        "rpc"

// Size of MSG_TYPE_RPC, note this includes the trailing '\0',
// i.e., strlen(MSG_TYPE_RPC) + 1
#define MSG_TYPE_RPC_LEN    sizeof(MSG_TYPE_RPC)

//! General-purpose communication facility
/**
 * Net provides low-level communication services that can be used for 
 * implementing various protocols. Net is used for providing the underlying
 * communication and synchronization facilities of the CommStage RPC service. 
 * Net uses TCP socket for communication and epoll facilities for selecting
 * available sockets. Net is implemented in a fully asyncrhonous (non-
 * blocking approach)
 *  * Implementation of the lower networking layer that provides
 * the basic communication services utilized by CommStage RPC service. Net
 * is flexible and can be used for implementing various types of message
 * passing communication protocols, not only RPC.
 * <p>
 * Net uses epoll - a Linux-specific mecahnism - for socket management
 * on both the send and receive seides of the connection sockets. Net
 * has two internal threads the ensure independent progress of send and
 * receive operatins. Epoll has two modes of socket event generation:
 * level and edge triggered. Net uses edge triggered, which facilitates
 * asynchronous processing.
 * <p>
 * A main concept in Net's implementation is that all communication is
 * asynchronous. All sockets are set in non-blockig mode and as soon as
 * a socket is unable to process more data, Net's send/receive threads yield
 * the CPU until an epoll event indicating availability of the socket is
 * received.
 * <p>
 *
 * @author Longda
 * @date   5/20/07
 */

class DataThreadParam
{
public:
    DataThreadParam()
    {
        MUTEX_INIT(&mutex, NULL);
        COND_INIT(&cond, NULL);
    }

    ~DataThreadParam()
    {
        if (sockQ.empty() == false)
        {
            LOG_ERROR("sockQ isn't empty");
        }
        MUTEX_DESTROY(&mutex);
        COND_DESTROY(&cond);
    }

public:
    pthread_t          tid;
    pthread_mutex_t    mutex;
    pthread_cond_t     cond;
    std::deque<int>    sockQ;

};

class Net
{
public:
    //! Enumeration for operation return codes
    /**
     * Most of Net methods return completion status codes listed in this
     * enumeration.
     */
    typedef enum
    {
        SUCCESS = 0,            //!< successful completion
        NET_ERR_ASETUP,//!< Net has already been setup by caller
        NET_ERR_LISTENER,//!< error in setup listener socket at server
        NET_ERR_THREAD,//!< error in starting send and receive threads
        NET_ERR_EPOLL,//!< error reported by epoll descriptors
        NET_ERR_PIPE,//!< error in pipe descriptor
        NET_ERR_SHUTDOWN,//!< error in shutdown
        NET_ERR_ASHUTDOWN,//!< Net has already been shutdown
        NET_ERR_CONNECT,//!< error in connecting to server
        NET_ERR_DISCONNECT,//!< error in disconnecting
        NET_ERR_CONN_NOTFOUND,//!< connection not found
        NET_ERR_CONN_EXISTS,//!< connection already exists
        NET_ERR_SRVPORT,//!< server port required
        NET_ERR_SRVCB//!< connection callback required by server
    }status_t;

    //! Enumeration for the connection callback invocation cause
    /**
     * The callback is invoked when a new connection is connected or an
     * existing diconnected. The values of this enumeration are passed as
     * additional context to the connection callback.
     */


    //! Enumeration for callback return codes
    /** 
     * When a new socket connection is established either at server or 
     * client side, Net invokes a user callback to setup the connection 
     * appropriately.
     */
    enum
    {
        CB_SUCCESS = 0,         //!< callback completed successfully
        CB_ERROR                //!< callback failed
    };

public:

    Net(Stage *commStage);
    virtual ~Net();

    /**
     * setup network
     */
    virtual int setup();

    //! Shutdown Net infrastructure
    virtual int shutdown();

    //! Get a connection by end point 
    /**
     * Returns a connection Conn object for the remote end point. If the
     * connection already exists, this method is a simple look up in Net's
     * connection manager. If the connection does not exist, a new
     * connection is created and a TCP socket is established to the
     * server running at end point. If the caller has context whether the
     * lookup is performed on the server side, the optional parameter
     * serverSide can be set to true. This will prevent an attempt to 
     * create a connection back to the client if the client has disconnected
     * the scoket (hence the connection deleted) before the server responds. 
     *
     * @param[in]   ep          remote end point
     * @param[in]   serverSide  whether this connection is called on the server
     * @return a Conn object providding communication to ep
     */
    Conn* getConn(EndPoint& ep, bool serverSide = false, int sock = -1);

    //! Delete a connection
    /**
     * Removes a connection from Net's connection manager by provifing the
     * remote end point of the connection object.
     *
     * @param[in]   ep  connection remote end point
     * return       status (SUCCESS, NET_ERR_CONN_NOTFOUND)
     */
    Net::status_t delConn(EndPoint& ep);

    //! Is Net object initialized
    /**
     * @return true if the Net object is initialized, false otherwise
     */
    bool initialized();

    //! Is Net object finalized
    /**
     * @return true if the Net object has been finalized, false otherwise
     */
    bool finalized();


    //! Adds a socket to the send epoll selector
    /**
     * Adds a socket to the send epoll file descriptor
     * 
     * @param[in]   sock    socket to be added to the send selector
     * @return      status (SUCCESS, NET_ERR_EPOLL)
     */
    Net::status_t addToSendSelector(int sock);
    void          delSendSelector(int sock);

    //! Adds a socket to the receive epoll selector
    /**
     * Adds a socket to the receive epoll file descriptor
     * 
     * @param[in]   sock    socket to be added to the receive selector
     * @return      status (SUCCESS, NET_ERR_EPOLL)
     */
    Net::status_t addToRecvSelector(int sock);
    void          delRecvSelector(int sock);

    //! Add a new connection
    /**
     * Add an already created, initialized, and connected Conn object to
     * the Net's connection manager.
     * <p>
     * This method is called internally from the RecvThread when a request for
     * a new connection arrives.
     *
     * @param[in]   conn    connection to be added
     * @param[in]   ep      end point for the connection
     * @param[in]   sock    socket of the connection
     */
    void addConn(Conn* conn, EndPoint& ep, int sock);


    //! Remove a connection from ConnMgr
    /**
     * This is a helper method for removing connections. In addition to
     * calling ConnMgr's remove method, it also calls the connection
     * callback
     *
     * @param[in]   conn    connection to be removed 
     * @return      error status
     */
    Net::status_t removeConn(int sock);

    ConnMgr& getConnMgr();

    void prepareSend(int sock, Conn *conn = NULL);
    void prepareRecv(int sock);

private:


    //! Copy constructor
    /**
     * Made private in order to avoid accidental use of the default one
     */
    Net(const Net &net);

    //! Assignment operator
    /**
     * Made private in order to avoid accidental use of the default one
     */
    Net& operator= (const Net& net);

protected:
    enum
    {
        THREAD_INFO_LEN = 8,    //!< size of thread notification message
        MAX_EPOLL_EVENTS = 64,    //!< maximum epoll events at a time
        ERR_BUF_SIZE = 256    //!< size of error buffer
    };

    /**
     * Setup send and receive epoll selectors
     */
    int setupSelectors();

    /**
     * Starts the threads that implement the send and receive communication
     * logic of the network layer.
     */
    int  startThreads();
    int  startDataThread(int threadIndex, bool isSend);
    void cleanupThreads();

    /*
     * net send data thread
     *
     * SendEPoolThread --> EPool thread, receive epoll event
     * SendThread -->  thread to sending data,
     *            -->  encapsulate sending function,
     *            -->  and it is static function
     * sending    -->  do the job of SendThread, sending all connection's data
     * sendData   -->  send one connection's data
     */
    static void* SendEPollThread(void* arg);
    static void* SendThread(void *arg);
           void  sending(int threadIndex);
           void  sendData(int sock);

    /**
     * net receive data thread
     *
     * RecvThread --> EPool thread, receive epoll event
     * RecvThread --> thread to recving data,
     *            --> encapsulate recving function,
     *            --> and it is static function
     * recving    --> do the job of RecvThread, recving all connection's data
     * recvData   --> recving one connection's data
     */
    static void* RecvEPollThread(void* arg);
    static void* RecvThread(void *arg);
           void  recving(int threadIndex);
           void  recvData(int sock);

    //! Accept sockets
    /**
     * A helper function called by the RecvThread for accepting the client
     * socket requests.
     */
    virtual void acceptConns();

    //! Remove connections not active for the longest time
    /**
     * @return the number of removed connections
     */
    size_t removeInactive();

    Stage *getCommStage();

protected:
    std::vector<DataThreadParam *>         sendDataThreads;
    std::vector<DataThreadParam *>         recvDataThreads;

    pthread_t recvThreadId, sendThreadId;   //!< receive and send thread id's
    int       recvPfd[2],   sendPfd[2];     //!< notification pipe descriptors
    int       recvEpfd,     sendEpfd;       //!< receive and send epoll file descriptors

    pthread_mutex_t netMutex;              //!< mutex lock
    bool            initFlag;              //!< flag indicating if Net has been initialized
    bool            shutdownFlag;          //!< flag indicating if Net has been finalized
    ConnMgr         connMgr;               //!< connection manager object

    Stage          *mCommStage;
};

#endif // _NETUTIL_HXX_
