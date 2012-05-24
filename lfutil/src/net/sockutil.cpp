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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/sendfile.h>

#include <iomanip>
#include <iostream>
#include <sstream>

// Include files
#include "net/sockutil.h"
#include "trace/log.h"

//! Implementation of Sock - socket facilities
/**
 * @file
 * @author Longda
 * @date   4/22/07
 *
 * Implementation of the socket facilities class. This class contains a 
 * number of static methods implemening basic socket and epoll  operations.
 */
 
extern int errno;

Sock::status_t
Sock::setNonBlocking(int fd)
{
    int rc, flags;
    // Get the current flags
    if((flags = fcntl(fd, F_GETFL, 0)) < 0)
        flags = 0;
    // Add the non blocking IO flag 
    rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if(rc < 0)
        return ERR_SOCK_SETOPT;
    else
        return SUCCESS;
}

Sock::status_t
Sock::setNoDelay(int sock)
{
    int nodelay = 1, sz = sizeof(int);
    int rc = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sz);
    if(rc < 0)
        return ERR_SOCK_SETOPT;
    else
        return SUCCESS;
}

Sock::status_t
Sock::setReuseAddr(int sock)
{
    int opt = 1, sz = sizeof(int);
    int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sz);
    if(rc < 0)
        return ERR_SOCK_SETOPT;
    else
        return SUCCESS;
}

Sock::status_t
Sock::setKeepalive(int sock)
{
    int opt = 1, sz = sizeof(int);
    int rc = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &opt, sz);
    if(rc < 0)
        return ERR_SOCK_SETOPT;
    else
        return SUCCESS;
}

Sock::status_t
Sock::setBufSize(int sock, int sndsz, int rcvsz)
{
    int len = sizeof(int);
    int rc;

    if(sndsz > 0) {
        rc = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndsz, len);
        if(rc < 0)
            return ERR_SOCK_SETOPT;
    }

    if(rcvsz > 0) {
        rc = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsz, len);
        if(rc < 0)
            return ERR_SOCK_SETOPT;
    }

    return SUCCESS;
}

Sock::status_t
Sock::setCloExec(int sock)
{
    // Get the current fd flags
    int flags = fcntl(sock, F_GETFD, 0);
    if(flags < 0)
        flags = 0;

    if(fcntl(sock, F_SETFD, flags | FD_CLOEXEC) < 0)
        return ERR_SOCK_SETOPT;
    else
        return SUCCESS;
}

Sock::status_t
Sock::readBlocking(int fd, char *buf, int buflen, int& nread)
{
    int n_read, n_left = buflen;

    while(n_left > 0) {
        n_read = read(fd, buf, n_left);
        if(n_read < 0) {
            perror("Sock::readBlocking read error");
            return ERR_SOCK_READ;
        } else if(n_read == 0)
            break;
        n_left -= n_read;
        buf += n_read;
    }
    nread = buflen - n_left;

    return SUCCESS;
}

Sock::status_t 
Sock::writeBlocking(int fd, const char *buf, int buflen, int& nwritten)
{
    int n_written, n_left = buflen;

    while(n_left > 0) {
        n_written = write(fd, buf, n_left);
        if(n_written <= 0) {
            perror("Sock::writeBlocking write error");
            return ERR_SOCK_WRITE;
        }
        n_left -= n_written;
        buf += n_written;
    }
    nwritten = buflen - n_left;

    return SUCCESS;
}

Sock::status_t
Sock::setupListener(unsigned short port, int& listen_sock,
                    int sndBufSz, int rcvBufSz)
{
    int rc;
    struct sockaddr_in s_addr;

    listen_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(listen_sock < 0) {
        LOG_ERROR("Sock::setupListener: socket: %s\n", strerror(errno));
        return ERR_SOCK_CREATE;
    }


    setCloExec(listen_sock);

    memset((char *)&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(port);

    // Reuse addr so the server can be restarted immediately. Eliminates
    // CLOSE_WAIT state of the server side of the socket
    setReuseAddr(listen_sock);

    // Enable keepalive so we will be informed if the peer
    // node goes away before it can inform us.
    setKeepalive(listen_sock); 

    // Set send and receive buffer sizes before listen (see man 7 tcp)
    setBufSize(listen_sock, sndBufSz, rcvBufSz);

    rc = bind(listen_sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
    if(rc < 0) {
        LOG_ERROR("Sock::setupListener: bind: %s\n", strerror(errno));
        return ERR_SOCK_BIND;
    }

    // We could just use SOMAXCONN
    // rc = listen(listen_sock, SOMAXCONN);
    rc = listen(listen_sock, Sock::LISTEN_BACKLOG);
    if(rc < 0) {
        LOG_ERROR("Sock::setupListener: listen: %s\n", strerror(errno));
        return ERR_SOCK_LISTEN;
    }

    return SUCCESS;
}


Sock::status_t
Sock::setupUdListener(const char* path, int& listen_sock)
{
    int rc;
    struct sockaddr_un s_addr;
    listen_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(listen_sock < 0) {
        LOG_ERROR("Sock::setupListener: socket: %s\n", strerror(errno));
        return ERR_SOCK_CREATE;
    }

    // Set FD_CLOEXEC flag for the listen socket so it will be closed
    // automatically in child when a child is forked. This prevents
    // multiple processes listening on the same port.
    // There is a time window between socket() and fcntl() in which
    // the fd will be inherited by forked child, but we ignore the window.
    setCloExec(listen_sock);

    unlink(path);
    memset((char *)&s_addr, 0, sizeof(s_addr));
    s_addr.sun_family = AF_LOCAL;
    strcpy(s_addr.sun_path, path);

    // Reuse addr so the server can be restarted immediately. Eliminates
    // CLOSE_WAIT state of the server side of the socket
    //setReuseAddr(listen_sock);

    // Enable keepalive so we will be informed if the peer
    // node goes away before it can inform us.
    //setKeepalive(listen_sock);

    rc = bind(listen_sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
    if(rc < 0) {
        LOG_ERROR("Sock::setupUdListener: bind: %s\n", strerror(errno));
        return ERR_SOCK_BIND;
    }
    // In the Linux implementation, sockets which are visible in the
    // filesystem honour the permissions of the directory they are in.
    // Their owner, group and their permissions can be changed. Creation
    // of a new socket will fail if the process does not have write and
    // search (execute) permission on the directory the socket is created
    // in. Connecting to the socket object requires read/write permission.
    // This behavior differs from many BSD-derived systems which ignore
    // permissions for Unix sockets.
    // Portable programs should not rely on this feature for security.
    chmod(path, 0777);
    
    rc = listen(listen_sock, Sock::LISTEN_BACKLOG);
    if(rc < 0) {
        LOG_ERROR("Sock::setupListener: listen: %s\n", strerror(errno));
        return ERR_SOCK_LISTEN;
    }
    return SUCCESS;
}

Sock::status_t
Sock::createSelector(int notifyFd, int& epfd)
{
    struct epoll_event ev;

    epfd = epoll_create(EPOLL_FDESC);
    if(epfd < 0) {

        LOG_ERROR("Sock::createSelector:can't create epoll fd\n");
        return Sock::ERR_EPOLL_CREATE;
    }

    // Add the notification fd to the epoll fd list
    // Using the default level triggering (EPOLLET is Edge Triggering
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = notifyFd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, notifyFd, &ev) < 0) {

        LOG_ERROR("Sock::createSelector:can't add notifyFd socket, rc:%d:%s\n",
                errno, strerror(errno));
        return ERR_EPOLL_ADD;
    }

    return SUCCESS;
}

Sock::status_t
Sock::addToSelector(int sock, dir_t dir, int epfd) 
{
    struct epoll_event ev;
    
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLET;    // Edge triggering for async comm
    if(dir == DIR_IN)
        ev.events |= EPOLLIN;
    else
        ev.events |= EPOLLOUT;
    ev.data.fd = sock;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev) < 0) {
        LOG_ERROR("Failed to add sock to epoll, rc :%d:%s", errno, strerror(errno));
        return ERR_EPOLL_ADD;
    } 

    return SUCCESS;
}

Sock::status_t
Sock::rmFromSelector(int sock, int epfd)
{
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL))
        return ERR_EPOLL_DEL;

    return SUCCESS;
}

Sock::status_t
Sock::connectTo(const char* hostname, unsigned int port, int& sock)
{
    int rc = create(PF_INET, SOCK_STREAM, 0, sock);
    if (rc != SUCCESS)
        return ERR_SOCK_CREATE;

    return connectTo(sock, hostname, port);
}

Sock::status_t
Sock::connectTo(int sock, const char* hostname, unsigned int port)
{
    struct sockaddr_in s_addr;
    struct hostent sent_struct, *sent = NULL;
    int rc, err;
    char buff[HOST_BUF_SIZE];

    // using reentrant version of gethostbyname()
    rc = gethostbyname_r(hostname, &sent_struct, 
            buff, HOST_BUF_SIZE, &sent, &err);
    if((rc) || (sent == NULL)) {
        return ERR_SOCK_RESOLVE;
    }
    memset((char *)&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    memcpy(&s_addr.sin_addr.s_addr, sent->h_addr, sent->h_length);
    s_addr.sin_port = htons(port);

    // Enable keepalive so we will be informed if the peer
    // node goes away before it can inform us.
    setKeepalive(sock);

    rc = connect(sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
    if(rc < 0) {
        if (errno != EINPROGRESS)
            return ERR_SOCK_CONNECT;

        return SOCK_CONNECTING;
    }

    return SUCCESS;
}

Sock::status_t
Sock::connectToUdServer(const char* path, int& sock)
{
    struct sockaddr_un s_addr;
    int rc;
    memset((char *)&s_addr, 0, sizeof(s_addr));
    s_addr.sun_family = AF_LOCAL;
    strcpy(s_addr.sun_path, path);

    // Enable keepalive so we will be informed if the peer
    // node goes away before it can inform us.
    setKeepalive(sock);
    rc = connect(sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
    if(rc < 0) {
        return ERR_SOCK_CONNECT;
    }
    return SUCCESS;
}

Sock::status_t
Sock::netToPrintAddr(const struct in_addr* netAddr, std::string& prtAddr)
{
    char buff[INET_ADDRSTRLEN];

    prtAddr = inet_ntop(AF_INET, netAddr, buff, INET_ADDRSTRLEN);

    return (prtAddr.empty() ? ERR_SOCK_INVALID : SUCCESS);
}

Sock::status_t
Sock::hostToIpAddr(const char *hostname, std::string& ipAddr)
{
    struct hostent hostent_struct, *hostent = NULL;
    char buff[HOST_BUF_SIZE];
    int rc = 0, err = 0;

    // using reentrant version of gethostbyname()
    rc = gethostbyname_r(hostname, &hostent_struct,
                         buff, HOST_BUF_SIZE, &hostent, &err);
    if((rc) || (hostent == NULL)) {
        return ERR_SOCK_RESOLVE;
    }
    struct in_addr *p = (struct in_addr *)hostent->h_addr;
    netToPrintAddr(p, ipAddr);

    return SUCCESS;
}

Sock::status_t
Sock::hostToIdDigits(const char *hostname, unsigned int digits, 
                     std::string& idStr)
{
    struct hostent hostent_struct, *record = NULL;
    char buff[HOST_BUF_SIZE];
    int rc = 0, err = 0;
    in_addr * address;

    // using reentrant version of gethostbyname()
    rc = gethostbyname_r(hostname, &hostent_struct,
                         buff, HOST_BUF_SIZE, &record, &err);
    if ((rc) || (record == NULL)) {
        return ERR_SOCK_RESOLVE;
    }

    address = (in_addr *)record->h_addr;
    int ip = ntohl(address->s_addr);
     
    std::ostringstream oss;
    oss << std::hex << std::setw(digits) << std::setfill('0') << ip;
    idStr.assign(oss.str());

    return SUCCESS;
}

Sock::status_t
Sock::getSockName(int sock, std::string& ipaddr, unsigned short& port)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int rc = getsockname(sock, (struct sockaddr *)&addr, &addrlen);
    if(rc != 0) 
        return ERR_SOCK_INVALID;
    struct in_addr *p = (struct in_addr *)&addr.sin_addr.s_addr;
    netToPrintAddr(p, ipaddr);
    port = ntohs(addr.sin_port);

    return SUCCESS;
}

Sock::status_t
Sock::getPeerName(int sock, std::string& ipaddr, unsigned short& port)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int rc = getpeername(sock, (struct sockaddr *)&addr, &addrlen);
    if(rc != 0) 
        return ERR_SOCK_INVALID;
    struct in_addr *p = (struct in_addr *)&addr.sin_addr.s_addr;
    netToPrintAddr(p, ipaddr);
    port = ntohs(addr.sin_port);

    return SUCCESS;
}

Sock::status_t
Sock::create(int domain, int type, int protocol, int& sockfd)
{
    sockfd = socket(domain, type, protocol);
    if (sockfd < 0)
        return ERR_SOCK_CREATE;
    return SUCCESS;
}

int
Sock::sendfile(int out_fd, const char *path, off_t *offset, size_t count)
{
   int fd = open(path, O_RDONLY);
   if (fd < 0)
   {
       LOG_ERROR("Failed to open file %s, rc %d:%s", path, errno, strerror(errno));
       return -1;
   }

   int sendCount = ::sendfile(out_fd, fd, offset, count);

   close(fd);

   return sendCount;
}

int Sock::sendfile(int sock, const char *filename, const unsigned long long offset,
        const unsigned long long count, long long *sendCount)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        *sendCount = 0;
        return errno != 0 ? errno : EACCES;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        *sendCount = 0;
        return errno != 0 ? errno : EACCES;
    }

    if (flags & O_NONBLOCK)
    {
        if (fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) == -1)
        {
            *sendCount = 0;
            return errno != 0 ? errno : EACCES;
        }
    }

    int   result = 0;
    off_t sendOffset = offset;

#ifdef LINUX
    /*
     result = 1;
     if (setsockopt(sock, SOL_TCP, TCP_CORK, &result, sizeof(int)) < 0)
     {
     logError("file: "__FILE__", line: %d, " \
                    "setsockopt failed, errno: %d, error info: %s.", \
                    __LINE__, errno, STRERROR(errno));
     close(fd);
     *total_send_bytes = 0;
     return errno != 0 ? errno : EIO;
     }
     */

    u64_t remain = count;

    while (remain > 0)
    {
        u64_t sended = 0;
        if (remain > ONE_GIGA)
        {
            sended = ::sendfile(sock, fd, &sendOffset, ONE_GIGA);
        }
        else
        {
            sended = ::sendfile(sock, fd, &sendOffset, remain);
        }
        if (sendCount <= 0)
        {
            result = errno != 0 ? errno : EIO;
            break;
        }

        remain -= sended;
    }

    *sendCount = count - remain;
#else
#ifdef OS_FREEBSD
    if (sendfile(fd, sock, offset, count, NULL, NULL, 0) != 0)
    {
        *sendCount = 0;
        result = errno != 0 ? errno : EIO;
    }
    else
    {
        *sendCount = count;
        result = 0;
    }
#endif
#endif

    if (flags & O_NONBLOCK)  //restore
    {
        if (fcntl(sock, F_SETFL, flags) == -1)
        {
            result = errno != 0 ? errno : EACCES;
        }
    }

    close(fd);
    return result;
}
