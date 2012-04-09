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
 * lprocess.h
 *
 *  Created on: Mar 26, 2012
 *      Author: Longda Feng
 */


#ifndef _L_PROCESS_H__
#define _L_PROCESS_H__



//! Get process Name
/**
 * @param[in]   progFullName  process full name with full path
 * @return      processName   process name without directory path
 */
std::string getProcessName(const char *progFullName);
//! Runs the service as a daemon
/**
 * Backgrounds the calling service as a system daemon by detaching it from
 * the controlling terminal, closes stdin, and reopens stdout and stderr
 * to the files specified in the input parmaters. "/dev/null" is accepted as
 * a valid input, which will be equivalent to closing the respective stream.
 * Keeping the streams open but reopening them allows the streams of the
 * controling terminal to be closed, thus making it possible for the terminal
 * to exit normally while the service is backgrounded. The same file
 * could be used for reopening both stderr and stdout streams.
 * Creates a new session and sets the service process as the group parent.
 *
 * @param[in]   stdOutFile  file to redirect stdout to (could be /dev/null)
 * @param[in]   stdErrFile  file to redirect stderr to (could be /dev/null)
 * @return  0 if success, error code otherwise
 */
int daemonizeService(const char *stdOutFile, const char *stdErrFile);

void sysLogRedirect(const char *stdOutFile, const char *stdErrFile);


#endif
