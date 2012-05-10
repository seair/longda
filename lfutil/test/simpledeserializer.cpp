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
 * simpledeserializer.cpp
 *
 *  Created on: May 9, 2012
 *      Author: Longda Feng
 */

#include "simpledeserializer.h"

#include "trace/log.h"

#include "comm/request.h"
#include "comm/response.h"

CSimpleDeserializer::CSimpleDeserializer()
{
    // TODO Auto-generated constructor stub

}

CSimpleDeserializer::~CSimpleDeserializer()
{
    // TODO Auto-generated destructor stub
}

void* CSimpleDeserializer::deserializeRequest(const char *buffer, int bufLen)
{
    Request *request = new Request();

    int rc = request->deserialize(buffer, bufLen);
    if (rc)
    {
        LOG_ERROR("Failed to deserialize Request");
        delete request;
        return NULL;
    }

    return request;
}

void* CSimpleDeserializer::deserializeResponse(const char *buffer, int bufLen)
{
    Response *response = new Response();

    int rc = response->deserialize(buffer, bufLen);
    if (rc)
    {
        LOG_ERROR("Failed to deserialize Response");
        delete response;
        return NULL;
    }

    return response;
}

void* CSimpleDeserializer::deserialize(const char *buffer, int bufLen)
{
    s32_t type = *(s32_t *)buffer;

    switch(type)
    {
    case MESSAGE_BASIC_REQUEST:
        return deserializeRequest(buffer, bufLen);
    case MESSAGE_BASIC_RESPONSE:
        return deserializeResponse(buffer, bufLen);
    default:
        LOG_ERROR("Unsupport type");
        return NULL;
    }

    return NULL;
}

