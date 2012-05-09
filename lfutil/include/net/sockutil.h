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

#ifndef _SOCKUTIL_HXX_
#define _SOCKUTIL_HXX_

//! Socket facilities
/**
 * @author Longda
 * @date   4/22/07
 *
 * This class contains a number of static methods implemening basic socket
 * and epoll maintenance operations. This class is not an abstraction 
 * for a socket connection. The Conn class provides such a higher-level 
 * abstraction.
 */
class Sock
{
public:
    //! Enumeration for error codes
    /**
     * This type is used for enumerating the error codes that the methods in
     * the socket utilities class return to their callers.
     */
    typedef enum
    {
        SUCCESS = 0,        //!< successful completion
        SOCK_CONNECTING,    //!< socket is connecting
        ERR_SOCK_CREATE,    //!< error in creating socket
        ERR_SOCK_RESOLVE,   //!< error in resolving hostname
        ERR_SOCK_BIND,      //!< error in binding a socket to a server address
        ERR_SOCK_LISTEN,    //!< error in listening on a socket
        ERR_SOCK_ACCEPT,    //!< error in accepting a socket connection request
        ERR_SOCK_CONNECT,   //!< error in client socket connect operation
        ERR_SOCK_INVALID,   //!< invalid socket
        ERR_SOCK_SETOPT,    //!< error in setting socket options
        ERR_SOCK_READ,      //!< error in reading from a socket
        ERR_SOCK_WRITE,     //!< error in writing to a socket
        ERR_SOCK_DISCONNECT,     //!< error in disconnecting socket
        ERR_EPOLL_CREATE,   //!< error in creating an epoll descriptor
        ERR_EPOLL_ADD,      //!< error in adding a socket to an epoll descriptor
        ERR_EPOLL_DEL       //!< error in removing a socket from an epoll fd
    } status_t;

    //! Enumeration for epoll event generation
    /**
     * The values of this enumeration are used to distinguish whether the socket
     * should generate read/incoming or write/outgoing epoll events
     */
    typedef enum
    {
        DIR_IN = 0,     //!< read epoll events
        DIR_OUT         //!< write epoll events
    } dir_t;

    //! Enumeration for different constants in the Sock namespace
    enum
    {
        DISCONNECTED = -1,   //!< indicates that the socket is disconnected
        SOCK_TIMEOUT = 30,   //!< socket timeout
        LISTEN_BACKLOG = 4096, //!< length of listen backlog socket queue
        EPOLL_FDESC = 1024, //!< number of epoll socket descriptors
        HOST_BUF_SIZE = 4096, //!< auxiliary data buf size in gethostbyname_r
        SOCK_SEND_BUF_SIZE = 262144, //!< size of socket write kernel buffers
        SOCK_RECV_BUF_SIZE = 16384   //!< size of socket read kernel buffers
    };

public:
    //! Set a socket in non-blocking mode
    /**
     * Sets the blocking mode of a file descriptor to non-blocking.
     * The Network layer uses non-blocking communication only.
     *
     * @param[in]   fd  socket descriptor to be modified
     * @return          SUCCESS
     *
     * @post    fd is in non-blocking mode
     */
    static Sock::status_t setNonBlocking(int fd);

    //! Disable the Naggle aggregation algorithm
    /**
     * This disables the Naggle aggregation algorithm, which is enabled by default
     * on all TCP sockets. Disabling the algorithm improves the communication
     * latency of short message transfers.
     *
     * @param[in]   sock    socket descriptor to be modified
     * @return              SUCCESS, ERR_SOCK_SETOPT
     *
     * @post    sock has its Naggle mechanism disabled
     */
    static Sock::status_t setNoDelay(int sock);

    //! Enabled reuse of server ports
    /**
     * By default ports on which a server-side socket has listened to cannot be
     * reused for a subsequent bind operation for a duration of 2 minutes. This
     * is a socket mechanism to prevent delayed packet duplicas to be forwarded to
     * the new socket. We disable this mechanism to allow a server to rebind
     * quickly to the same port. The default delay is mainly intended for noisy
     * and alow networks.
     *
     * @param[in]   sock    socket descriptor to be modified
     * @return              SUCCESS, ERR_SOCK_SETOPT
     *
     * @post a new socket can bind to the same address/port after sock is closed
     */
    static Sock::status_t setReuseAddr(int sock);

    //! Enable the keepalive timer.
    /**
     * Keepalive allows us to detect that the peer node has gone away without
     * informing us.  We need this to properly failover to running services,
     * otherwise we will hang.  The other option is to use a comm stage timeout.
     *
     * @param[in]   sock    socket descriptor to be modified
     * @return              SUCCESS, ERR_SOCK_SETOPT
     *
     * @post a new socket can bind to the same address/port after sock is closed
     */
    static Sock::status_t setKeepalive(int sock);

    //! Set the kernel send and receive buffers
    /**
     * Changes the size of the socket kernel buffers. Larger buffers lead to higher
     * effective throughput of long messages. The side effect is larger use of
     * kernel resources. Care should be taken so that the size is not increased
     * too much when highly scalable communication is anticipated, e.g., hundreds
     * and thousands of simultaneously opened sockets.
     *
     * @param[in]   sock    socket descriptor to be modified
     * @param[in]   sndsz   new size of socket kernel send buffer
     * @param[in]   rcvsz   new size of socket kernel receive buffer
     * @return              SUCCESS, ERR_SOCK_SETOPT
     *
     * @post sock's buffer sizes are modified to the value of the size parameter
     *       if the corresponding size parameter is larger than zero
     */
    static Sock::status_t setBufSize(int sock, int sndsz, int rcvsz);

    //! Set FD_CLOEXEC for a socket

    /**
     * Set FD_CLOEXEC flag for the listen socket so it will be closed
     * automatically in child when a child is forked. This prevents
     * multiple processes listening on the same port.
     * There is a time window between socket() and fcntl() in which
     * the fd will be inherited by forked child, but we ignore the window.
     *
     * File descriptor will not be inherited by child if FD_CLOEXEC flag is set.
     * To avoid any long-run child process take over socket from parent, all
     * socket should get the flag set right after the fd is created.
     *
     *
     * @param[in]   sock    socket descriptor to be modified
     * @return              SUCCESS, ERR_SOCK_SETOPT
     *
     * @post socket will be closed on exec (no inheritance)
     */
    static Sock::status_t setCloExec(int sock);

    //! Creates an epoll file descriptor (selector)
    /**
     * Creates an epoll file descriptor for selecting multiple sockets. Epoll
     * facilitates implementation of a non-blocking event-driven communication
     * strategy. The notification file descriptor is used to inform the thread
     * listening for epoll events to exit.
     *
     * @param[in]   notifyFd    pipe fd for notifying the epoll listen thread
     * @param[out]  epfd        created epoll file descriptor
     * @return                  SUCCESS, ERR_EPOLL_CREATE, ERR_EPOLL_ADD
     *
     * @pre     notifyFd is a valid socket or a pipe descriptor
     * @post    epfd has notifyFd in its fd set generating read events
     */
    static Sock::status_t createSelector(int notifyFd, int& epfd);

    //! Adds a socket to an epoll selector
    /**
     * Adds a socket to an epoll selector and enables event generation in read
     * ot write mode. The socket is set to generate events in
     * Edge Triggering (ET) mode.
     *
     * @param[in]   sock    socket to be added to the epoll selector
     * @param[in]   dir     direction for event generation (in/read or out/write)
     * @param[in]   epfd    epoll selector
     * @return              SUCCESS, ERR_EPOLL_ADD
     *
     * @pre     sock and epfd are valid socket and epoll file descriptors
     * @post    sock is added to epfd with ET event generation in direction
     * specified by dir
     */
    static Sock::status_t addToSelector(int sock, Sock::dir_t dir, int epfd);

    //! Creates, binds, and listens to a INET socket
    /**
     * A utility methods for aggregating the steps for setting up a server side
     * socket. Performs socket creation, binding to address,port and listens to
     * the socket for incoming TCP socket client requests.
     *
     * @param[in]   port        port socket to listen to
     * @param[out]  listen_sock listening socket ready to accept connections
     * @param[in]   sndBufSz    size of socket kernel send buffer
     * @param[in]   rcvBufSz    size of socket kernel receive buffer
     * @return                  SUCCESS
     *
     * @post    listen_sock is ready to accept new client connections
     */
    static Sock::status_t setupListener(unsigned short port, int& listen_sock,
            int sndBufSz, int rcvBufSz);

    //! Creates, binds, and listens to a unix domain socket
    /*
     * A utility methods for aggregating the steps for setting up a unix domain server
     * side socket. Performs socket creation, binding to address, path and listens to
     * the socket for incoming unix domain socket client request.
     *
     * @param[in]	path		path unix domain socket to listen to
     * @param[out]	listen_sock	listening socket ready to accept connections
     * @return					SUCESS
     *
     * @post	listen_sock is ready to accept new client connections
     */
    static Sock::status_t setupUdListener(const char* path, int& listen_sock);

    //! Connects a socket to a server
    /**
     * A utility method that returns a newly created TCP socket connected to the
     *  server host and port.
     *
     * @param[in]   hostname    name or IP address of server
     * @param[in]   port        TCP port of server to connect to
     * @param[out]  sock        connected socket
     * @return      SUCCESS, ERR_SOCK_CREATE, ERR_SOCK_BIND, ERR_SOCK_LISTEN
     *
     * @post    sock is connected to the server and is ready for communication
     */
    static Sock::status_t connectTo(const char* hostname, unsigned int port,
            int& sock);

    static Sock::status_t connectTo(int sock, const char* hostname,
            unsigned int port);

    //! Connect a unix domain socket to a server
    /*
     * A Utility method that return a newly created UD socket connected to the
     * server host and port.
     *
     * @param[in]	path		UD path of server's listen socket bind to
     * @param[out]	sock		connected socket
     * @return		SUCCESS, ERR_SOCK_CREATE, ERR_SOCK_BIND, ERR_SOCK_LISTEN
     *
     * @post	sock is connected to the server and is ready for communication
     */
    static Sock::status_t connectToUdServer(const char* path, int& sock);

    //! Blocking read
    /**
     * Reads the specified number of bytes from a socket in blocking mode.
     *
     * @param[in]   fd      file descriptor (socket) to read from
     * @param[in]   buf     pointer to pre-allocated buffer
     * @param[in]   buflen  length of data to be read from socket in buffer
     * @param[out]  nread   number of actually read bytes
     * @return              SUCCESS, ERR_SOCK_READ
     *
     * @pre sock is connected
     * @pre buf is a valid pointer to a buffer with size of at least buflen bytes
     * @post nread number of bytes are read from the socket and written in buf
     */
    static Sock::status_t readBlocking(int fd, char* buf, int buflen,
            int& nread);

    //! Blocking write
    /**
     * Writes the specified number of bytes to the provided socket in blocking mode.
     *
     * @param[in]   fd       file descriptor (socket) to write into
     * @param[in]   buf      pointer to pre-allocated buffer
     * @param[in]   buflen   length of data to be written from buffer to socket
     * @param[out]  nwritten number of actually written bytes
     * @return               SUCCESS, ERR_SOCK_WRITE
     *
     * @pre sock is connected
     * @pre buf is a valid pointer to a buffer with size of at least buflen bytes
     * @post nwritten number of bytes are read from buf and written to sock
     */
    static Sock::status_t writeBlocking(int fd, const char *buf, int buflen,
            int& nwritten);

    //! Convert an IP address from binary representation to human-readable string
    /**
     * A helper function to convert an IP address into human-readable string format
     *
     * TODO: support IP v6
     *
     * @param[in]   netAddr     IP address binary representation
     * @param[out]  prtAddr     human-readable IP string
     * @return                  SUCCESS, ERR_SOCK_INVALID
     */
    static Sock::status_t netToPrintAddr(const struct in_addr* netAddr,
            std::string& prtAddr);

    //! Resolves a host name and converts it into an IP address string
    /**
     * A helper function to convert a hostname to its IP address in a string format
     *
     * @param[in]   hostname    hostname to be resolved
     * @param[out]  ipAddr      IP address for hostname
     * @return                  SUCCESS, ERR_SOCK_RESOLVE
     */
    static Sock::status_t hostToIpAddr(const char *hostname,
            std::string& ipAddr);

    //! Resolves a host name and converts it into an IP address string in hex
    /**
     * A helper function to convert a hostname to its IP address in hex format
     *
     * @param[in]   	hostname    hostname to be resolved
     * @param[digits]   digits      ID string length
     * @param[out]  	idStr       the ID string
     * @return                  SUCCESS, ERR_SOCK_RESOLVE
     */
    static Sock::status_t hostToIdDigits(const char *hostname,
            unsigned int digits, std::string& idStr);

    //! Obtains the IP address and port of the local end of a TCP socket
    /**
     * Obtains the local IP address and port from a connected socket.
     *
     * @param[in]   sock    connected socket
     * @param[out]  ipaddr  local IP address in string format
     * @param[out]  port    local port for socket
     * @return              SUCCESS, ERR_SOCK_INVALID
     *
     * @pre sock is a valid connected socket
     */
    static Sock::status_t getSockName(int sock, std::string& ipaddr,
            unsigned short& port);

    //! Obtains the IP address and port of the remote end of a TCP socket
    /**
     * Obtains the iremote IP address and port from a connected socket.
     *
     * @param[in]   sock    connected socket
     * @param[out]  ipaddr  iremote IP address in string format
     * @param[out]  port    remote port for socket
     * @return              SUCCESS, ERR_SOCK_INVALID
     *
     * @pre sock is a valid connected socket
     */
    static Sock::status_t getPeerName(int sock, std::string& ipaddr,
            unsigned short& port);

    // wrap the socket() call
    static Sock::status_t create(int domain, int type, int protocol,
            int& sockfd);

    static int sendfile(int out_fd, const char *path, off_t *offset, size_t count);
    /**
     * sendfile
     */
    static int sendfile(int sock, const char *filename, const unsigned long long offset,
            const unsigned long long count, long long *sendCount);

private:

    //! Constructor
    /**
     * Private constructor to prevent instantiation of this class.
     * Sock is a collection of static methods.
     */
    Sock();

    //!  Private destructor
    ~Sock();
};

#endif // _SOCKUTIL_HXX_
