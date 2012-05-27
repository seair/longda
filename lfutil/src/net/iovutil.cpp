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
 * iovutil.cpp
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */

#include "lang/lstring.h"
#include "seda/timerstage.h"
#include "trace/log.h"
#include "os/mutex.h"
#include "io/io.h"

#include "comm/message.h"
#include "comm/request.h"
#include "comm/response.h"
#include "comm/packageinfo.h"
#include "comm/commevent.h"
#include "comm/commdataevent.h"

#include "net/conn.h"
#include "net/net.h"
#include "net/netex.h"
#include "net/endpoint.h"


bool checkAttachFile(MsgDesc& md)
{
    if (md.attachFileLen == 0 && md.attachFilePath.empty() == true)
    {
        return true;
    }else if (md.attachFileLen == 0 && md.attachFilePath.size() != 0)
    {
        LOG_WARN("attach file length is 0, but file path %s is not empty",
                md.attachFilePath.c_str());
        return true;
    }else if(md.attachFileLen != 0 && md.attachFilePath.empty() == true)
    {
        return false;
    }else
    {
        // md.attachFileLen != 0 && md.attachFilePath.size() !=0
        u64_t fileSize = 0;
        if (getFileSize(md.attachFilePath.c_str(), fileSize) != 0)
        {
            return false;
        }

        if (fileSize < md.attachFileOffset + md.attachFileLen)
        {
            return false;
        }

        return true;
    }
    return false;
}

IoVec* makeRpcMessage(MsgDesc& md)
{

    // Find out the total size of the attachments
    int attLen = 0;
    for (u32_t i = 0; i < md.attachMems.size(); i++)
    {
        attLen += md.attachMems[i]->size;
    }

#if RPC_HEAD_USE_STRING
    std::string attLenStr = CLstring::sizeToPadStr(attLen, HDR_NUM_PRECISION);

    int msgLen = md.message->getSerialSize();
    std::string msgLenStr = CLstring::sizeToPadStr(msgLen, HDR_NUM_PRECISION);

    if (checkAttachFile(md) == false)
    {
        LOG_ERROR("Check attach file error");
        return NULL;
    }

    std::string fileLenStr = CLstring::sizeToPadStr(md.attachFileLen, HDR_NUM_PRECISION);

    int msgHdrLen = sizeof(PackHeader) + msgLen;
    char* hdr = new char[msgHdrLen];
    if (hdr == NULL)
    {
        LOG_ERROR("Failed to alloc %d memory", msgHdrLen);
        return NULL;
    }
    PackHeader *pHdr = (PackHeader *)hdr;
    pHdr->setHeader(MSG_TYPE_RPC, msgLenStr.c_str(),
            attLenStr.c_str(), fileLenStr.c_str());

#else
    int msgLen = md.message->getSerialSize();
    int msgHdrLen = sizeof(PackHeader) + msgLen;
    char* hdr = new char[msgHdrLen];
    if (hdr == NULL)
    {
        LOG_ERROR("Failed to alloc %d memory", msgHdrLen);
        return NULL;
    }
    PackHeader *pHdr = (PackHeader *) hdr;
    strcpy(pHdr->mType, MSG_TYPE_RPC);
    pHdr->mMsgLen = msgLen;
    pHdr->mAttLen = attLen;
    pHdr->mFileLen = md.attachFileLen;

#endif

    int rc = md.message->serialize(hdr + sizeof(PackHeader),  msgLen);
    if (rc)
    {
        LOG_ERROR("Failed to serialize the message");
        delete hdr;
        return NULL;
    }

    // Sending the Net::HDR_LEN header + the message XML string in one op
    IoVec* iov = new IoVec(hdr, msgHdrLen, Conn::sendCallback, 0);
    if (iov == NULL)
    {
        LOG_ERROR("Failed to alloc memory IoVec ");
        delete hdr;
        return NULL;
    }
    return iov;
}

int prepareIovecs(MsgDesc &md, IoVec** iovs, CommEvent* cev)
{
    // Prepare a buffer with the header and message
    iovs[0] = makeRpcMessage(md);
    if (!iovs[0])
    {
        LOG_ERROR("No memory to make rpc message");
        return -1;
    }

    bool success = true;
    int i = 0;
    int attBlockCount = md.attachMems.size();
    for (; i < attBlockCount; i++)
    {
        // The memory pointed to by base is user allocated
        iovs[i + 1] = new IoVec(md.attachMems[i], IoVec::USER_ALLOC);
        if (iovs[i + 1] == NULL)
        {
            LOG_INFO("Failed to alloc one IoVec");
            success = false;
        }
        iovs[i + 1]->setCallback(Conn::sendCallback, 0);
    }

    if (success == false)
    {
        for (int j = 0; j < i + 1; j++)
        {
            iovs[j]->cleanup();
            delete iovs[j];
        }

        return -1;
    }

    return 0;
}

int prepareReqIovecs(MsgDesc &md, IoVec** iovs, CommEvent* cev, Conn *conn, Stage *cs)
{
    int rc = prepareIovecs(md, iovs, cev);
    if (rc)
    {
        return rc;
    }

    // In case of a failure, the iovec's will be flushed and sent to the send
    // callback with a failure state. At this point, we need to complete the
    // CommEvent causing these iovecs. For this reason, the last iovec is
    // set with callback parameter the event pointer. Only one iovec is
    // updated as only one of them needs to complete the event.
    cb_param_t* cbp = new cb_param_t;
    memset(cbp, 0, sizeof(cb_param_t));
    cbp->cev = cev;
    cbp->cs = cs;
    cbp->conn = conn;
    cbp->reqId = cev->getRequestId();
    cbp->fileOffset   = md.attachFileOffset;
    cbp->fileLen  = md.attachFileLen;
    strncpy(cbp->filePath, md.attachFilePath.c_str(), sizeof(cbp->filePath) - 1);
    iovs[md.attachMems.size()]->setCallback(Conn::sendCallback, cbp);

    return 0;
}

int prepareRespIovecs(MsgDesc &md, IoVec** iovs, CommEvent* cev, Stage *cs)
{
    int rc = prepareIovecs(md, iovs, cev);
    if (rc)
    {
        return rc;
    }

    // We cannot complete the event after send returns because send is
    // asynchronous and may not have sent all data from the buffers when it
    // returns, which will be often the case for long attachments.
    // We will pass the event pointer to the sendCallback with the
    // last IoVec associated with this event. The sendCallback will
    // complete (free) the event. Also, in case of a connection failure,
    // the last vector will be called with CLEANUP or ERROR state and will
    // complete the event.
    cb_param_t* cbp = new cb_param_t;
    memset(cbp, 0, sizeof(cb_param_t));
    cbp->cev = cev;
    cbp->cs = cs;
    iovs[md.attachMems.size()]->setCallback(Conn::sendCallback, cbp);

    return 0;
}

IoVec* prepareRecvHeader(Conn* conn, Stage *cs)
{

    PackHeader *hdr = new  PackHeader();
    if (hdr == NULL)
    {
        LOG_ERROR("Failed to alloc memory for Package Header");
        return NULL;
    }

    cb_param_t* cbp = new cb_param_t;
    if (cbp == NULL)
    {
        LOG_ERROR("Failed to alloc memory for cb_param_t");
        delete hdr;
        return NULL;
    }
    memset(cbp, 0, sizeof(cb_param_t));
    cbp->conn = conn;
    cbp->cs   = cs;

    IoVec* iov = new IoVec(hdr, sizeof(PackHeader), Conn::recvCallback, cbp);
    if (iov == NULL)
    {
        LOG_ERROR("Failed to allocate an Iovec");
        delete cbp;
        delete hdr;
        return NULL;
    }

    /**
     * @@@ FIXME maybe need handle cbp
     */

    return iov;
}
