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
 * iovutil.h
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */

#ifndef IOVUTIL_H_
#define IOVUTIL_H_


bool checkAttachFile(MsgDesc& md);
/**
 * make rpc header and msg block
 */
IoVec* makeRpcMessage(MsgDesc& md);

/**
 * prepare Iovecs buffer for send data
 */
int prepareIovecs(MsgDesc &md, IoVec** iovs, CommEvent* cev);

/**
 * prepare Iovecs buffer for send request
 */
int prepareReqIovecs(MsgDesc &md, IoVec** iovs, CommEvent* cev, Conn *conn, Stage *cs);

/**
 * prepare Iovecs buffer for send response
 */
int prepareRespIovecs(MsgDesc &md, IoVec** iovs, CommEvent* cev, Stage *cs);

IoVec* prepareRecvHeader(Conn* conn, Stage *cs);

#endif /* IOVUTIL_H_ */
