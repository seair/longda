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
 * commdataevent.h
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */

#ifndef COMMDATAEVENT_H_
#define COMMDATAEVENT_H_

#include <seda/stageevent.h>

class CommSendEvent : public StageEvent
{
public:
    CommSendEvent(void *conn):
        mConn(conn)
    {

    }
    ~CommSendEvent(){}

public:
    void *mConn;        // this is Conn*
};

class CommRecvEvent: public StageEvent
{
public:
    CommRecvEvent(int sock): mSocket(sock) {}
    ~CommRecvEvent(){}

public:
    int mSocket;
};


#endif /* COMMDATAEVENT_H_ */
