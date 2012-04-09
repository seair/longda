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
 * @ func:  provide project pid functions
 */


#ifndef __PIDFILE_H__
#define __PIDFILE_H__



//! Generates a PID file for the current component
/**
 * Gets the process ID (PID) of the calling process and writes a file
 * dervied from the input argument containing that value in a system
 * standard directory, e.g. /var/run/progName.pid
 *
 * @param[in] programName as basis for file to write
 * @return    0 for success, error otherwise
 */
int writePidFile(const char *progName);

//! Cleanup PID file for the current component
/**
 * Removes the PID file for the current component
 *
 */
void removePidFile(void);



#endif // __PIDFILE_H__

