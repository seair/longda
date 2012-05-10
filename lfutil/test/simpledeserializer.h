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
 * simpledeserializer.h
 *
 *  Created on: May 9, 2012
 *      Author: Longda Feng
 */

#ifndef SIMPLEDESERIALIZER_H_
#define SIMPLEDESERIALIZER_H_

#include "lang/serializable.h"

/*
 *
 */
class CSimpleDeserializer : public Deserializable
{
public:
    CSimpleDeserializer();
    virtual ~CSimpleDeserializer();

    void* deserialize(const char *buffer, int bufLen);

protected:
    void* deserializeRequest(const char *buffer, int bufLen);
    void* deserializeResponse(const char *buffer, int bufLen);
};

#endif /* SIMPLEDESERIALIZER_H_ */
