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
 * packageinfo.h
 *
 *  Created on: Apr 28, 2012
 *      Author: Longda Feng
 */

#ifndef PACKAGEINFO_H_
#define PACKAGEINFO_H_


enum
{
    HDR_NUM_PRECISION = 7,
    HDR_TYPE_LEN = 8,
    HDR_MSG_LEN = HDR_NUM_PRECISION + 1,
    HDR_ATT_LEN = HDR_NUM_PRECISION + 1,
    HDR_FILE_LEN = HDR_NUM_PRECISION + 1,
    HDR_LEN = HDR_TYPE_LEN + HDR_MSG_LEN + HDR_ATT_LEN + HDR_FILE_LEN,
    HDR_TYPE_POS = 0,
    HDR_MSG_LEN_POS = HDR_TYPE_POS + HDR_TYPE_LEN,
    HDR_ATT_LEN_POS = HDR_MSG_LEN_POS + HDR_MSG_LEN,
    HDR_FILE_LEN_POS = HDR_ATT_LEN_POS + HDR_ATT_LEN
};

typedef struct _packHeader
{
    _packHeader()
    {
        memset(this, 0, sizeof(*this));
    }

    char        mType[HDR_TYPE_LEN];
    char        mMsgLen[HDR_MSG_LEN];
    char        mAttLen[HDR_ATT_LEN];
    char        mFileLen[HDR_FILE_LEN];
    void setHeader(const char* hdrType,
                   const char* msgLen,
                   const char* attLen,
                   const char* fileLen)
    {

        memset(this, 0, sizeof(struct _packHeader));

        strncpy(mType, hdrType, HDR_TYPE_LEN - 1);
        strncpy(mMsgLen, msgLen, HDR_NUM_PRECISION);
        strncpy(mAttLen, attLen, HDR_NUM_PRECISION);
        strncpy(mFileLen, attLen, HDR_NUM_PRECISION);

    }
}PackHeader __attribute__ ((aligned(1)));

#endif /* PACKAGEINFO_H_ */
