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
 * endpoint.h
 *
 *  Created on: Apr 24, 2012
 *      Author: Longda Feng
 */

#ifndef ENDPOINT_H_
#define ENDPOINT_H_

#include "defs.h"
#include "lang/serializable.h"

#define MAX_HOSTNAME_LEN       32
#define MAX_LOCATION_LEN       32
#define MAX_SERVICE_LEN        16


class EndPoint : public Serializable
{
public:
    EndPoint();
    EndPoint(s32_t port);
    EndPoint(const char *hostname, s32_t port);
    EndPoint(const char *hostname, const char *location, s32_t port);
    EndPoint(const char *hostname, const char *location,
             const char *service, s32_t port);
    ~EndPoint();


    const char* getHostName() const;
    void        setHostName(const char *hostname);

    const char* getLocation() const;
    void        setLocation(const char *location);
    void        setDefaultLocation(const char *location);

    const char* getService() const;
    void        setService(const char *service);

    s32_t       getPort() const;
    void        setPort(s32_t port);

    /**
     * inherit Serializable functions
     */
    int serialize(char *buffer, int bufferLen);
    int deserialize(const char *buffer, int bufferLen);
    int getSerialSize();
    void toString(std::string &output);

    //! Create a string just contain hostname and port
    /**
     * The string is used as a key in the EndPoint to socket map.
     *
     * @param[in]   ep  end point
     * @return  key string
     */
    void toHostPortStr(std::string &output);


    static const s32_t  DEFAULT_PORT = -1;
    static const char   DEFAULT_LOCATION[MAX_LOCATION_LEN];
    static const char   DEFAULT_SERVICE[MAX_SERVICE_LEN];
private:
    char        mHostName[MAX_HOSTNAME_LEN];
    char        mLocation[MAX_LOCATION_LEN];
    char        mService[MAX_SERVICE_LEN];
    s32_t       mPort;
};


#endif /* ENDPOINT_H_ */
