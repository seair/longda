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
 * netex.h
 *
 *  Created on: Apr 24, 2012
 *      Author: Longda Feng
 */

#ifndef NETEX_H_
#define NETEX_H_

#include "net/net.h"

//! Exception for new connection creation operations
/**
 * In general, Net and its related Conn and ConnMgr classes return status
 * codes from their methods and don't use exceptions. This exception
 * is used to convey richer status information to callers from connect
 * operations when a large number of differnt types of errors may occur.
 */
struct NetEx {
    int         code;           //!< exception error code
    std::string message;        //!< exception error message

    //! Exception consrtuctor
    /**
     * @param[in]   status  error code
     * @param[in]   msg     error message as string
     */
    NetEx(int status, std::string& msg) {
        code = status;
        message = msg;
    }

    //! Exception consrtuctor
    /**
     * @param[in]   status  error code
     * @param[in]   msg     error message as array of characters
     */
    NetEx(int status, const char* msg) {
        code = status;
        message = msg;
    }
};

#endif /* NETEX_H_ */
