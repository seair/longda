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
 * request.h
 *
 *  Created on: Apr 28, 2012
 *      Author: Longda Feng
 */

#ifndef REQUEST_H_
#define REQUEST_H_

#include "comm/message.h"
#include "net/endpoint.h"

class Request : public Message
{
public:
    Request():
        Message(MESSAGE_BASIC_REQUEST)
    {

    }

    enum
    {
        MAX_PROTOCAL_LEN = 32
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

        mSourceEp.serialize(temp, bufferLen - Message::getSerialSize());
        temp += mSourceEp.getSerialSize();

        strncpy(temp, mProtocal, MAX_PROTOCAL_LEN);
        temp[MAX_PROTOCAL_LEN - 1] = '\0';

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

        mSourceEp.deserialize(temp, bufferLen - Message::getSerialSize());
        temp += mSourceEp.getSerialSize();

        strncpy(mProtocal, temp, MAX_PROTOCAL_LEN);
        mProtocal[MAX_PROTOCAL_LEN - 1] = '\0';

        return 0;
    }

    int getSerialSize()
    {
        int size = Message::getSerialSize();

        size += mSourceEp.getSerialSize();
        size += sizeof(mProtocal);

        return size;
    }

    void toString(std::string &output)
    {
        Message::toString(output);

        output += ",";
        mSourceEp.toString(output);

        output += ",protocal:";
        output += mProtocal;

        return ;
    }

public:
    EndPoint mSourceEp;
    char     mProtocal[MAX_PROTOCAL_LEN];



};



#endif /* REQUEST_H_ */
