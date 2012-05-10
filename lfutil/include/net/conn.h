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


#ifndef _CONN_HXX_
#define _CONN_HXX_

#include <pthread.h>
#include <deque>

#include "trace/log.h"
#include "io/selectdir.h"
#include "lang/serializable.h"
#include "seda/stage.h"

#include "net/endpoint.h"
#include "net/iovec.h"
#include "net/sockutil.h"

#include "comm/packageinfo.h"
#include "comm/commevent.h"
#include "comm/message.h"
#include "comm/request.h"
#include "comm/response.h"




//! Connection abstraction 
/**
 * @author Longda
 * @date   5/05/07
 */

class Conn;


//! Structure for passing information to receive callback
/**
 * This structure keeps the context of the incoming message needed by
 * the receive callback for associating the different packet segments
 * with one message.
 */
typedef struct _cb_param_t
{
    void reset()
    {
        memset(this, 0, sizeof(struct _cb_param_t));
    }
    Conn       *conn;        //!< connection on which message is arriving
    Stage      *cs;          //!< pointer to the CommStage instance
    CommEvent  *cev;         //!< event on which current processing is being done
    bool        eventDone;   //!< indicates whether event has been completed
    u32_t       reqId;       //<! request id of the client event
    u32_t       remainVecs;  //!< in ATTACH/FILE/DRAIN stage, remain vec number
    u32_t       attLen;      //<! total length of all attachments
    u64_t       fileLen;     //<! attach file length
    u64_t       fileOffset;  //<! file offset
    char        filePath[FILENAME_LENGTH_MAX];    //<! file path
    u64_t       drainLen;    //<! drain length
}cb_param_t;

/**
 *
 * Conn manages the context of the TCP socket connections between communicating
 * components. This includes creating and destroying socket connections, 
 * sending and receiving data, managing receive and send queues, keeping
 * track of connection's state.
 */
class Conn {
public:
    //! Enumeration for operation status
    /**
     * The majority of Conn methods other than member setters and accessors 
     * return a status code indicating how the requested operation completed.
     */
    typedef enum {
        SUCCESS = 0,            //!< operation completed successfully
        CONN_READY,             //!< connection is still ready
        CONN_CONNECTING,        //!< connect is underway
        CONN_ERR_CREATE,        //!< error in create socket
        CONN_ERR_CONNECT,       //!< error in connect operation
        CONN_ERR_DISCONNECT,    //!< error in disconnect operation
        CONN_ERR_NOVEC,         //!< there is no Iovec in queue
        CONN_ERR_NOMEM,         //!< there is no memory
        CONN_ERR_NOCEV,         //!< these is no CommEvent
        CONN_ERR_MISMATCH,      //!< version isn't match
        CONN_ERR_BROKEN,        //!< connection is broken
        CONN_ERR_WRITEFILE,     //!< write file failed
        CONN_ERR_BUSY,          //!< conn is busy
        CONN_ERR_UNAVAIL        //!< connection is busy (would block)
    } status_t;

    //! Enumeration for connection receive status
    /**
     * Conn is used for implementing higher level communication services, such
     * as RPC and other protocols. Typically, these have packet formats 
     * including headers and payloads. Conn is intended to support a variety
     * of such protocols. This enumeration allows Conn to process such 
     * composite packets and carry context between header and payload
     * processing. 
     */
    typedef enum {
        HEADER = 0,             //!< processing packet header
        MESSAGE,                //!< processing packetg control part
        ATTACHMENT,             //!< data payload (attachment)
        ATTACHFILE,             //!< attach file
        DRAIN                   //!< draining the socket
    } nextrecv_t;

    //! Enumeration for the reason when a connection is closed
    /**
     * This type enumeration is used by the cleanup() method for indicating
     * the reason for closing the connection. Based on this information
     * the IoVec's in the send and receive queues are completed
     * appropriately.
     */
    typedef enum {
        ON_CLEANUP = 0,         //!< connection is closed on cleanup
        ON_ERROR                //!< connection is closed on error
    } cleanup_t;

    enum {
        VEC_BATCH_NUM = 5
    };

    typedef enum
    {
        ON_CONNECT = 0,         //!< callback is invoked during connect
        ON_DISCONNECT         //!< callback is invoked during disconnect
    } conn_t;

    //! Type definition of connection callback
    /**
     * @param[in]   conn    connection whose socket is disconnected
     * @param[in]   param   opaque callback context
     *
     * @return status
     */
    typedef bool (*conncb_t)(Conn* conn, void* param);


public:
    //! Constructor
    Conn();

    //! Destructor
    ~Conn();

    //! Connect to a remote end point
    /**
     * Connects the module in which the object is instantiated to a remote
     * end point at which a server is expected to listen to for incoming
     * connection requests.
     *
     * @param[in]   ep      remote end point (host, port)
     * @param[out]  sock    local socket opened for the connection
     * @return      status (SUCCESS, CONN_ERR_CONNECT)
     */
    Conn::status_t connect(EndPoint& ep, int& sock);

    /**
     * connection callback after accept a client or connect to server
     */
    int connCallback(Conn::conn_t state);

    //! Set socket
    /**
     * Sets connection's socket internal member and changes the socket options
     * such as kernel buffers, blocking mode, and Naggle switch off
     *
     * @param[in]   sock    socket
     */
    void setup(int sock);

    //! Disconnect a connection
    /**
     * Disconnects a connected connection. Performs all necessary object
     * and socket cleanup.
     *
     * @return      status (SUCCESS, CONN_ERR_DISCONNECT)
     */
    Conn::status_t disconnect();

    //! Clean up connection
    /**
     * Cleans up internal state and socket. If other callers have acuired the
     * connection, cleanup() will block until all such references are
     * released. Send and receive IoVec queues are emptied. The callbacks
     * of each IoVec are called with vector state IoVec::state_t CLEANUP.
     *
     * @param[in]   how inidicates how the connections is being cleaned up
     *
     * @return true if connection has been cleaned up, false if another thread
     * has started clean up and will complete it.
     */
    int cleanup(cleanup_t how);

    //! Releases connection
    /**
     * Decrements the connection's reference counter. Used to keep track of
     * references to the same connection in order to avoid race conditions.
     */
    void release();

    //! Acquire connection
    /**
     * Increments the connections's reference countera. Used to keep track of
     * references to the same connection in order to avoid race conditions.
     */
    void acquire();

    //! Return if the connection is not referenced
    bool isIdle();

    //! Send an array of IoVec
    /**
     * Sends an array of IoVec objects. The method is non-blocking. When the
     * method returns, the caller has no guarantee about the completion
     * status of any of the input IoVec objects. Each vector's completion
     * information is provided to the callback registerd to the vector.
     * Before any send operation is attempted, all vectors are queued to
     * the connection's send queue. The vectors are queued in the same order
     * as the elements of the input array. They will be completed in this
     * order as well.
     * <p>
     * For small number of short vectors, it is possible that all data
     * associated with the vectors is send before the socket becomes busy.
     * The return code in this case would be SUCCESS. If at least one
     * of the vectors is not completed because the socket would block, the
     * return code will be CONN_ERR_UNAVAIL. This does not mean that the
     * operation has failed. Instead, that the not all vectors have been
     * completed.
     *
     * @param[in]   numVecs     number of vectors in the vector array
     * @param[in]   msgVecs     array of IoVec objects
     * @return      status (SUCCESS, CONN_ERR_UNAVAIL)
     *
     * @pre connection is connected
     * @post all elements in msgVecs are enqueued in connection's send queue.
     * @post processing of one or more vectors may have started
     * @post one or more IoVec objects may have been completed
     */
    Conn::status_t send(int numVecs, IoVec* msgVecs[]);
    Conn::status_t send(IoVec* msgVecs);

    //! Posts an array of vectors to send queue
    /**
     * This method queues the input vectors to the connection's send queue.
     * The vectors become eligible for transmission, but there is no
     * guarantee whether any of them will be started.
     *
     * @param[in]   numVecs     number of vectors in the vector array
     * @param[in]   msgVecs     array of IoVec objects
     * @return      status (SUCCESS)
     */
    Conn::status_t postSend(int numVecs, IoVec* msgVecs[]);

    //! Posts one IoVec to the send queue
    /**
     * Semantics is the same as postSend with an array of vectors.
     */
    Conn::status_t postSend(IoVec* msgVec);

    //! Makes send progress
    /**
     * Invokes the internal method for progress on the currently active
     * IoVec. If this vector is completed, the first vector from the send
     * queue is removed and set as the active send vector.
     * Return success if there are no more vectors in the send queue and
     * the active vector has been completed.
     *
     * @param[in] ready     is the Conn's socket ready for send
     * @return  status (SUCCESS, CONN_ERR_UNAVAIL)
     */
    Conn::status_t sendProgress(bool ready = false);

    //! Start receiving an array ov vectors
    /**
     * Vectors will be internally queued on the receive queue in the order
     * prsent in the array. They will be completed in the same order.
     * If there is no active vector, the first element in the array will become
     * the active vector.
     *
     * @param[in]   numVecs     number of elements in vector array
     * @param[in]   msgVecs     array of vectors
     * @return  status (SUCCESS, CONN_ERR_UNAVAIL)
     */
    Conn::status_t recv(int numVecs, IoVec* msgVecs[]);

    //! Start receiving a vector
    /**
     * The input vector is queued on the receive queue. If there is is no
     * active receive vector, msgVec will be set as the active vector, and
     *
     * @param[in]   msgVec  vector to be received into
     * @return  status (SUCCESS, CONN_ERR_UNAVAIL)
     */
    Conn::status_t recv(IoVec* msgVec);

    //! Post an array of vectors to receive queue
    /**
     * @param[in]   numVecs     number of elements in the array
     * @param[in]   msgVecs     array of vectors to be posted
     * @return      status (SUCCESS)
     */
    Conn::status_t postRecv(int numVecs, IoVec* msgVecs[]);

    //! Post a vector to receive queue
    /**
     * @param[in]   msgVec  vector to be posted
     * @return      status (SUCCESS)
     */
    Conn::status_t postRecv(IoVec* msgVec);

    //! Make progress on receive queue vectors
    /**
     * Tries to read from socket in the active vector if socket is ready.
     * if the active vector is completed, removes the head element from
     * the receive queue and sets it as active vector.
     *
     * @param[in] ready is the connection socket ready to receive
     * @return  status (SUCCESS, CONN_ERR_UNAVAIL)
     */
    Conn::status_t recvProgress(bool ready = false);

    //! Get conection socket
    /**
     * @return  conncetion's socket
     */
    int  getSocket();
    void setSock(int sock);

    //! Is connection connected
    /**
     * Checks if the connection object is connected.
     *
     * @return true if connection is connected, false otherwise
     */
    bool connected();

    //! get/set connection state
    void setState(Conn::status_t state);
    Conn::status_t getState();

    //! Get the next packet component to be received
    /**
     * @return  next packet component (HEADER, MESSAGE, ATTACH)
     */
    Conn::nextrecv_t getNextRecv();

    //! Set the next packet component to be received
    /**
     * @param[in]   nr  next packet component
     */
    void setNextRecv(nextrecv_t nr);

    //! Flow control management
    void messageOut();
    void messageIn();

    //! Update activity serial number
    void updateActSn();

    //! Get last activity serial number
    u64_t getActSn() const
    {
        return mActivitySn;
    }

    EndPoint &getPeerEp() { return mPeerEp; }
    void      setPeerEp(EndPoint &ep) { mPeerEp = ep; }

    void       addEventEntry(u32_t msgId, CommEvent *event);
    void       removeEventEntry(u32_t msgId);
    CommEvent* getAndRmEvent(u32_t msgId);


    //! Set default socket send/receive buffer size for non-listen connections
    static void setSndBufSz(int size);
    static void setRcvBufSz(int size);

    static int getSndBufSz();
    static int getRcvBufSz();

    //! Set default socket send/receive buffer size for listen connections
    static void setLsnSndBufSz(int size);
    static void setLsnRcvBufSz(int size);

    static int getLsnSndBufSz();
    static int getLsnRcvBufSz();

    static int  getSocketTimeout();
    static void setSocketTimeout(int timeout);

    static int getMaxBlockSize();
    static void setMaxBlockSize(int size);

    static void setSocketProperty(std::map<std::string, std::string> &section);

    static EndPoint&  getLocalEp();
    static bool       setLocalEp(std::map<std::string, std::string> &section, bool server);

    static void       setCommStage(Stage *commStage);


    static void       setDeserializable(Deserializable *deserializer);

    static Deserializable * getDeserializable();

    static void setSelectDir(CSelectDir *selectDir);

    static CSelectDir *getSelectDir();

    /**
     * receive callback after receive data
     */
    static int recvCallback(IoVec *iov, void *param, IoVec::state_t state);

    /**
     * send callback after send data
     */
    static int sendCallback(IoVec *iov, void *param, IoVec::state_t state);


protected:
    static int recvErrCb(IoVec *iov, cb_param_t* cbp, IoVec::state_t state, bool freeIov);

    static IoVec** allocIoVecs(const size_t baseLen, IoVec *iov, int &blockNum);

    static int repostIoVecs(Conn* conn, IoVec* iov, const size_t baseLen);

    static int repostIoVec(Conn* conn, IoVec* iov, size_t baseLen);

    static int recvHeaderCb(IoVec *iov, cb_param_t* cbp, IoVec::state_t state);

    static void prepare2Drain(IoVec *iov, cb_param_t* cbp, Conn *conn, u64_t leftSize);

    static void cleanMdAttach(MsgDesc &md);

    static int  allocAttachIoVecs(MsgDesc &md, const size_t baseLen);

    static int  pushAttachMessage(Conn *conn, MsgDesc &md, cb_param_t* cbp);

    static int  recvReqMsg(Request *req, IoVec *iov, cb_param_t* cbp, Conn *conn);

    static int  recvRspMsg(Response *rsp, IoVec *iov, cb_param_t* cbp, Conn *conn);

    static int  sendReqCallback(cb_param_t* cbp, IoVec::state_t state);

    static int  sendRspCallback(cb_param_t* cbp, IoVec::state_t state);

    static int recvPrepareFileIov(cb_param_t* cbp, Conn *conn);

    static int  recvPrepareRspAttach(MsgDesc &mdresp, cb_param_t* cbp, Conn *conn);

    static void generateRcvFileName(std::string &fileName, CommEvent *cev);

    static void eventDone(CommEvent *cev, Conn *conn);

    static int repostReusedIoVec(const size_t baseLen, IoVec *iov, cb_param_t* cbp);

    static void checkEventReady(bool eventReady, Conn *conn, IoVec *iov, cb_param_t *cbp);

    static void sendBadMsgErr(Conn* conn, CommEvent::status_t errCode, const char *errMsg);

    static int recvDrain(IoVec *iov, cb_param_t* cbp, Conn *conn);

    static int recvAttach(IoVec *iov, cb_param_t* cbp, Conn *conn);

    static int recvFile(IoVec *iov, cb_param_t* cbp, Conn *conn);

protected:

    //! Copy constructor
    /**
     * Private copy constructor to prevent inadvertent use of the default one
     */
    Conn(const Conn &conn);

    //! Assignment operator
    /**
     * Private assignment operator to prevent inadvertent use of the default one
     */
    Conn& operator=(const Conn &conn);

    //! Cleanup an IoVec
    /**
     * A utility to cleanup vectors after they are completed. It invokes the
     * vector's callback, if one is registered with the vector.
     *
     * @param[in]   iov vector to be finalized
     * @param[in]   how indiactes how the connection is being cleaned up
     */
    void cleanupVec(IoVec *iov, cleanup_t how);

    //! Make progress on active send vector
    /**
     * If the socket is ready for sending, data from the currently active
     * send IoVec will be sent until all vector data is transmitted or
     * the socket becomes busy (unavailable). In the former case, the vector
     * is completed and the vector's callback is invoked.
     * If the socket is busy, the method returns an appropriate error status.
     *
     * @return status of vector processing
     */
    status_t sendvecProgress();

    //! Make progress on active receive vector
    /**
     * If the socket is ready for receiving (data available for reading),
     * data from the socket is read and placed into the currently active
     * receive IoVec. If the vector is completed, the vector's callback is
     * invoked. If the socket would block, an appropriate error message
     * is rerturned.
     *
     * @return status of vector processing
     */
    status_t recvvecProgress();







private:
    struct msg_cntr_t
    {
        unsigned int in;
        unsigned int out;
        msg_cntr_t()
        {
            in = 0;
            out = 0;
        }
    };

    static int       gSocketSendBufSize;  //!< socket send buffer size
    static int       gSocketRcvBufSize;   //!< socket receive buffer size

    static int       gListenSendBufSize;  //!< listen socket send buffer size
    static int       gListenRcvBufSize;   //!< listen socket receive buffer size

    static int      gMaxBlockSize;          //!< one block buffer size

    static int       gTimeout;            //!< socket timeout

    static EndPoint  gLocalEp;            //!< local EndPoint

    static Stage    *gCommStage;          // !< CommStage instance

    static std::string gBaseDataPath;     //

    static Deserializable *gDeserializable; //  !< Deserializer

    static CSelectDir     *gSelectDir;


    int              mSock;              //!< connection socket

    nextrecv_t       mNextRecvPart;      //!< next component of received packet
    status_t         mConnState;         //!< connection state

    bool             mCleaning;          //!< connection is being cleaned up
    cleanup_t        mCleanType;         //!< cleanup type for disconnect

    int              mRefCount;          //!< reference count of connection references
    msg_cntr_t       mMsgCounter;        //!< counter for messages
    u64_t            mActivitySn;        //!< serial number of last connect activity

    static u64_t     globalActSn;        //!< serial number of connect activities
                                         // used to find non active connections to close
    EndPoint         mPeerEp;            //!< peer's EndPoint
    pthread_mutex_t  mMutex;             //!< mutex protecting the Conn members

    IoVec              *mCurSendBlock;  //!< current vector being sent
    std::deque<IoVec *> mSendQ;         //!< queue for vectors to be sent
    pthread_mutex_t     mSendMutex;     //!< send queue mutex
    bool                mReadyToSend;   //!< indicates that socket is ready for send

    IoVec              *mCurRecvBlock;  //!< current vector being received into
    std::deque<IoVec *> mRecvQ;         //!< queue of receive vectors
    pthread_mutex_t     mRecvMutex;     //!< receive queue mutex
    bool                mReadyRecv;     //!< flag that data is available for read
    cb_param_t         *mRecvCb;        //!< recv callback parameter, won't free until cleanup Connection

    std::map<u32_t, CommEvent*> mSendEventMap;        //!< map(requestID, send event)
    pthread_mutex_t             mEventMapMutex;




};

#endif // _CONN_HXX_

