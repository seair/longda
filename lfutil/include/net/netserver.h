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
 * netserver.h
 *
 *  Created on: Apr 24, 2012
 *      Author: Longda Feng
 */

#ifndef NETSERVER_H_
#define NETSERVER_H_

#include "net/net.h"

//! Communication server specialization class
/**
 * NetServer is sub-class of Net. It implements a specialization of the
 * parent class with specific behavior required for components
 * implementing a TCP communication sever.
 */
class NetServer : public Net {
public:
    NetServer(Stage *commStage);

    NetServer(Stage *commStage, unsigned short port);

    //! Destructor
    /**
     * Destroys the NetServer object
     */
    virtual ~NetServer();

    //! Set server port
    /**
     * @param[in]   port    server prot
     */
    void setPort(unsigned short port);

    //! Get port
    /**
     * @return  server port
     */
    unsigned short getPort() const;

    //! Get listener socket
    /**
     * @return socket for listening for new connections
     */
    int getListenSock() const;

    /**
     * override net functions
     */
    int setup();
    int shutdown();
    void acceptConns();


private:
    unsigned short mPort;        //!< port to listen on for new connections
    int mListenSock;             //!< socket for accepting new connections

};

#endif /* NETSERVER_H_ */
