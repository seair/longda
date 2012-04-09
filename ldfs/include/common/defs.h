// __CR__
// Copyright (c) 2008-2011 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__

/**
 * @ author: hustjackie@gmail.com
 * @ date:  2010/04/01
 * @ func:  provide project common defines
 */

#ifndef _DEFS_H_
#define _DEFS_H_

#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "version.h"

#ifndef gettid
#define gettid() syscall(SYS_gettid)
#endif

inline const std::string& theSwVersion()
{
    static const std::string swVersion(VERSION_STR);

    return swVersion;
}

enum
{
    //General Error Codes
    STATUS_SUCCESS              = 0,//!< Success status should be zero,
    STATUS_INVALID_PARAM,           //!< Invalid parameter
    STATUS_FAILED_INIT,             //!< Failed to init program
    STATUS_PROPERTY_ERR,            //!< Property error
    STATUS_INIT_LOG,                //!< log error
    STATUS_INIT_SEDA,               //!< seda init error
    STATUS_INIT_THREAD,             //!< failed to init thread
    STATUS_INIT_FDFS,               //!< failed to init fdfs
    STATUS_INIT_FDHT,               //!< failed to init fdht
    STATUS_FAILED_JOB,              //!< Failed to do job
    STATUS_FDFS_CONNECT,            //!< Connect FDFS failed
    STATUS_FDFS_CMD,                //!< FDFS CMD failed
    STATUS_FDHT_CONNECT,            //!< FDHT connect failed
    STATUS_FDHT_CMD,                //!< FDHT CMD failed


    
    STATUS_INIT_FUSE,               //!< Failed to init fuse
    STATUS_INIT_CACHE,              //!< Failed to init cache
    STATUS_TRANS_OBJID,             //!< Failed to convert path to objid
    STATUS_GET_METADATA,            //!< Failed to get get metadata
    STATUS_UNKNOW_ERROR,            //!< unknown error
    STATUS_LAST_ERR                 //!< last error code
    
};

const unsigned int ONE_KILO            = 1024;
const unsigned int ONE_MILLION         = ONE_KILO * ONE_KILO;
const unsigned int ONE_GIGA            = ONE_MILLION * ONE_KILO;
const unsigned int FILENAME_LENGTH_MAX = 256; //the max filename length
const unsigned int MAX_CONNECTION_NUM  = 12;

/*
 * Define types
 *
 */
typedef unsigned char       u8_t;
typedef unsigned short      u16_t;
typedef unsigned int        u32_t;
typedef unsigned long long  u64_t;

typedef char                s8_t;
typedef short               s16_t;
typedef int                 s32_t;
typedef long long           s64_t;

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#endif //_DEFS_H_

