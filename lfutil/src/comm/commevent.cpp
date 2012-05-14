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

#include "comm/commevent.h"
#include "comm/request.h"
#include "comm/response.h"
#include "net/sockutil.h"

//! Implementation of CommEvent
/**
 * @author Longda
 * @date   5/22/07
 *
 */

CommEvent::CommEvent() :
        status(SUCCESS), serverGen(false), sock(Sock::DISCONNECTED)
{
}

CommEvent::CommEvent(MsgDesc *req, MsgDesc *resp) :
        status(SUCCESS), serverGen(false), sock(Sock::DISCONNECTED)
{
    setRequest(req);
    setResponse(resp);
}

CommEvent::CommEvent(Message* reqMsg, Message* respMsg) :
        status(SUCCESS), serverGen(false), sock(Sock::DISCONNECTED)
{
    setRequestMsg(reqMsg);
    setResponseMsg(respMsg);
}

CommEvent::~CommEvent()
{
    request.cleanup();
    response.cleanup();
}

void CommEvent::setRequest(MsgDesc *req)
{
    if (req)
    {
        request = *req;
    }
    else
    {
        request.cleanup();
    }
}

void CommEvent::setRequestMsg(Message* reqMsg)
{
    request.cleanup();
    request.message = reqMsg;
}

void CommEvent::setResponse(MsgDesc *resp)
{
    if (resp)
    {
        response = *resp;
    }
    else
    {
        response.cleanup();
    }
}

void CommEvent::setResponseMsg(Message* respMsg)
{
    response.cleanup();
    response.message = respMsg;
}

MsgDesc& CommEvent::getRequest()
{
    return request;
}

Message*  CommEvent::getRequestMsg()
{
    return request.message;
}

MsgDesc& CommEvent::getResponse()
{
    return response;
}

Message* CommEvent::getResponseMsg()
{
    return response.message;
}

void CommEvent::setStatus(CommEvent::status_t stat)
{
    status = stat;
}

CommEvent::status_t CommEvent::getStatus()
{
    return status;
}

bool CommEvent::isfailed()
{
    return status != SUCCESS;
}

void CommEvent::setRequestId(unsigned int reqId)
{
    requestId = reqId;
}

unsigned int CommEvent::getRequestId()
{
    return requestId;
}

MsgDesc CommEvent::adoptRequest()
{
    // The caller now takes ownership of the request descriptor,
    // including the Message and the aray of vectors for the attachments
    // The caller is responsible for deallocating all adopted resources.
    MsgDesc md = request;

    request.cleanupContainer();

    return md;
}

MsgDesc CommEvent::adoptResponse()
{
    // The caller now takes ownership of the response descriptor,
    // including the Message and the aray of vectors for the attachments
    // The caller is responsible for deallocating all adopted resources.
    MsgDesc md = response;

    response.cleanupContainer();

    return md;
}

bool CommEvent::doneWithErrorResponse(CommEvent::status_t eventErrCode, int rspErrCode,
        const char *errmsg)
{
    status = eventErrCode;

    Response *response = new Response();
    if (response == NULL)
    {
        LOG_ERROR("Failed to alloc memory for Response");
        return false;
    }

    response->mStatus = rspErrCode;
    response->setErrMsg(errmsg);

    setResponseMsg(response);

    // IMPORTANT: This method calls done internally. This should be the very 
    // method called on the event object. The done() call will cause the
    // callback stack to be unrolled and all callbacks called in order.
    done();

    return true;
}

void CommEvent::completeEvent(CommEvent::status_t stev)
{
    status = stev;

    done();
}

void CommEvent::setServerGen()
{
    serverGen = true;
}

bool CommEvent::isServerGen()
{
    return serverGen;
}

