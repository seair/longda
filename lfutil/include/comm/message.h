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
 * message.h
 *
 *  Created on: Apr 28, 2012
 *      Author: Longda Feng
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "lang/serializable.h"
#include "trace/log.h"
#include "lang/lstring.h"


class Message : public Serializable
{
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

        char *temp = buffer;

        *(u32_t *) temp = mType;
        temp += sizeof(mType);

        *(u32_t *) temp = mVersion;
        temp += sizeof(mVersion);

        *(u64_t *) temp = mId;
        temp += sizeof(mId);

        return 0;
    }

    int deserialize(const char *buffer, int bufferLen)
    {
        if (bufferLen < getSerialSize())
        {
            LOG_ERROR("Not enough buffer to serialize");
            return -1;
        }

        const char *temp = buffer;

        mType = *(u32_t *) temp;
        temp += sizeof(mType);

        mVersion = *(u32_t *) temp;
        temp += sizeof(mVersion);

        mId = *(u64_t *) temp;
        temp += sizeof(mId);

        return 0;
    }

    int getSerialSize()
    {
        int size = Message::getSerialSize();

        size += sizeof(mType);
        size += sizeof(mVersion);
        size += sizeof(mId);

        return size;
    }

    void toString(std::string &output)
    {
        std::string tempStr;

        CLstring::valToStr(mType, tempStr);
        output += ("type:" + tempStr);

        CLstring::valToStr(mVersion, tempStr);
        output += ("version:" + tempStr);

        CLstring::valToStr(mId, tempStr);
        output += ("id:" + tempStr);

        return ;
    }

public:
    u32_t       mType;
    u32_t       mVersion;
    u64_t       mId;


};
#endif /* MESSAGE_H_ */
