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
 * serializable.h
 *
 *  Created on: Apr 25, 2012
 *      Author: Longda Feng
 */

#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_

class Deserializable
{
public:
    virtual ~Deserializable() = 0;
    /*
     * deserialize buffer to one object
     * @param[in]buffer,     buffer to store the object serialized bytes
     * @return *             object
     */
    virtual void* deserialize(const char *buffer, int bufLen) = 0;
};

class Serializable
{
public:
    virtual ~Serializable() = 0;

    /*
     * serialize this object to bytes
     * @param[in] buffer,    buffer to store the object serialized bytes,
     *                       please make sure the buffer is enough
     * @param[in] bufferLen, buffer length
     * @return,              used buffer length -- success, -1 means failed
     */
    virtual int serialize(char *buffer, int bufferLen) = 0;

    /*
     * deserialize bytes to this object
     * @param[in] buffer      buffer to store the object serialized bytes
     * @param[in] bufferLen   buffer lenght
     * @return                used buffer length -- success , -1 --failed
     */
    virtual int deserialize(const char *buffer, int bufferLen) = 0;

    /**
     * get serialize size
     * @return                >0 -- success, -1 --failed
     */
    virtual int getSerialSize() = 0;

    /**
     * this function will generalize one output string
     */
    virtual void toString(std::string &output) = 0;
};


#endif /* SERIALIZABLE_H_ */
