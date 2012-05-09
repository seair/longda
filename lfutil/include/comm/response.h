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
 * Response.h
 *
 *  Created on: Apr 28, 2012
 *      Author: Longda Feng
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include <string.h>

#include "comm/message.h"
#include "trace/log.h"

class Response : public Message
{
public:
    Response(){}
    Response(int status, const char *msg):
        mStatus(status)
    {
        memset(mErrMsg, 0, sizeof(mErrMsg));
        strncpy(mErrMsg, msg, MAX_ERROR_MSG_LEN -1);
    }
    void setErrMsg(const char *errMsg)
    {
        memset(mErrMsg, 0, sizeof(mErrMsg));
        strncpy(mErrMsg, errMsg, MAX_ERROR_MSG_LEN -1);
    }


    enum{
        MAX_ERROR_MSG_LEN = 128
    };

public:
    /*
     * inherit from serializble
     */
    int serialize(char *buffer, int bufferLen)
    {
        if (bufferLen < getSerialSize())
        {
            LOG_ERROR("Not enough buffer to serialize");
            return -1;
        }

        Message::serialize(buffer, bufferLen);

        char *temp = buffer + Message::getSerialSize();

        *(int *)temp = mStatus;
        temp += sizeof(mStatus);

        strncpy(temp, mErrMsg, MAX_ERROR_MSG_LEN);
        temp[MAX_ERROR_MSG_LEN - 1] = '\0';

        return 0;
    }

    int deserialize(const char *buffer, int bufferLen)
    {
        if (bufferLen < getSerialSize())
        {
            LOG_ERROR("Not enough buffer to serialize");
            return -1;
        }

        Message::deserialize(buffer, bufferLen);

        const char *temp = buffer + Message::getSerialSize();

        mStatus = *(int *) temp;
        temp += sizeof(mStatus);

        strncpy(mErrMsg, temp, MAX_ERROR_MSG_LEN);
        mErrMsg[MAX_ERROR_MSG_LEN - 1] = '\0';

        return 0;
    }

    int getSerialSize()
    {
        int size = Message::getSerialSize();

        size += sizeof(mStatus);
        size += sizeof(mErrMsg);

        return size;
    }

    void toString(std::string &output)
    {
        Message::toString(output);

        std::string statusStr;
        CLstring::valToStr(mStatus, statusStr);
        output += (",status:" + statusStr);

        output += ",errMsg:";
        output += mErrMsg;

        return ;
    }

public:
    int   mStatus;
    char  mErrMsg[MAX_ERROR_MSG_LEN];


};


#endif /* RESPONSE_H_ */
