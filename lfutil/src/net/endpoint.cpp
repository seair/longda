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
 * endpoint.cpp
 *
 *  Created on: Apr 25, 2012
 *      Author: Longda Feng
 */

#include <string>
#include <string.h>

#include "trace/log.h"
#include "lang/lstring.h"

#include "net/endpoint.h"
#include "net/lnet.h"

const char   EndPoint::DEFAULT_LOCATION[] = "default";
const char   EndPoint::DEFAULT_SERVICE[]  = "unknown";

EndPoint::EndPoint()
{
    EndPoint(DEFAULT_PORT);
}

EndPoint::EndPoint(s32_t port)
{
    std::string hostname("localhost");

    getHostname(NULL, hostname);

    EndPoint(hostname.c_str(), port);
}

EndPoint::EndPoint(const char *hostname, s32_t port)
{
    EndPoint(hostname, DEFAULT_LOCATION, port);
}

EndPoint::EndPoint(const char *hostname, const char *location, s32_t port)
{
    EndPoint(hostname, DEFAULT_LOCATION, DEFAULT_SERVICE, port);
}

EndPoint::EndPoint(const char *hostname, const char *location,
             const char *service, s32_t port)
{
    setHostName(hostname);
    setLocation(location);
    setService(service);
    setPort(port);
}

EndPoint::~EndPoint()
{

}

const char* EndPoint::getHostName() const
{
    return mHostName;
}

void EndPoint::setHostName(const char *hostname)
{
    memset(mHostName, 0, MAX_HOSTNAME_LEN);
    strncpy(mHostName, hostname, MAX_HOSTNAME_LEN - 1);
}

const char* EndPoint::getLocation() const
{
    return mLocation;
}

void EndPoint::setLocation(const char *location)
{
    memset(mLocation, 0, MAX_LOCATION_LEN);
    strncpy(mLocation, location, MAX_LOCATION_LEN - 1);
}

void EndPoint::setDefaultLocation(const char *location)
{
    if (strcmp(mLocation, DEFAULT_LOCATION) == 0)
    {
        setLocation(location);
    }
}

const char* EndPoint::getService() const
{
    return mService;
}

void EndPoint::setService(const char *service)
{
    memset(mService, 0, MAX_SERVICE_LEN);
    strncpy(mService, service, MAX_SERVICE_LEN - 1);
}

s32_t  EndPoint::getPort() const
{
    return mPort;
}

void EndPoint::setPort(s32_t port)
{
    mPort = port;
}

int EndPoint::serialize(char *buffer, int bufferLen)
{
    int usedBufLen = MAX_HOSTNAME_LEN + MAX_LOCATION_LEN +
            MAX_SERVICE_LEN + sizeof(s32_t);

    if (bufferLen < usedBufLen)
    {
        LOG_ERROR("Buffer %p isn't enough to serialize, length is %u, need %u",
                buffer, bufferLen, usedBufLen);
        return -1;
    }


    memset(buffer, 0, usedBufLen );

    strncpy(buffer, mHostName, MAX_HOSTNAME_LEN);
    buffer += MAX_HOSTNAME_LEN;

    strncpy(buffer, mLocation, MAX_LOCATION_LEN);
    buffer += MAX_LOCATION_LEN;

    strncpy(buffer, mService, MAX_SERVICE_LEN);
    buffer += MAX_SERVICE_LEN;

    *(s32_t *)buffer = mPort;

    return usedBufLen;
}

int EndPoint::deserialize(const char *buffer, int bufferLen)
{
    int usedBufLen = MAX_HOSTNAME_LEN + MAX_LOCATION_LEN +
                MAX_SERVICE_LEN + sizeof(s32_t);

    if (bufferLen < usedBufLen)
    {
        LOG_ERROR("Buffer %p isn't enough to deserialize, length is %u, need %u",
                buffer, bufferLen, usedBufLen);
        return -1;
    }


    setHostName(buffer);
    buffer += MAX_HOSTNAME_LEN;

    setLocation(buffer);
    buffer += MAX_LOCATION_LEN;

    setService(buffer);
    buffer += MAX_SERVICE_LEN;

    setPort(*(s32_t *)buffer);

    return usedBufLen;
}

int EndPoint::getSerialSize()
{
    return MAX_HOSTNAME_LEN + MAX_LOCATION_LEN +
            MAX_SERVICE_LEN + sizeof(s32_t);
}

void EndPoint::toString(std::string &output)
{

    output.reserve(getSerialSize());

    output = mHostName;
    output += ',';

    output += mLocation;
    output += ',';

    output += mService;
    output += ',';

    std::string port;
    CLstring::valToStr(mPort, port);

    output += port;

    return ;
}

void EndPoint::toHostPortStr(std::string &output)
{
    std::string portStr;
    CLstring::valToStr(mPort, portStr);

    output = ((std::string)mHostName + "@" + portStr);

    return;
}
