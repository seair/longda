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
 * lprocess.cpp
 *
 *  Created on: Mar 26, 2012
 *      Author: Longda Feng
 */

#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#include "trace/log.h"
#include "os/lprocess.h"

#define MAX_ERR_OUTPUT 100000    // 100k
#define MAX_STD_OUTPUT 100000    // 100k

#define RWRR (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

std::string getProcessName(const char *progName)
{
    std::string processName;

    int bufLen = strlen(progName);

    assert(bufLen);

    char *buf = new char[bufLen + 1];
    if (buf == NULL)
    {
        std::cerr << "Failed to alloc memory for program name."
                << SYS_OUTPUT_FILE_POS<< SYS_OUTPUT_ERROR << std::endl;
                return "";
    }
    memset(buf, 0, bufLen + 1);
    strncpy(buf, progName, bufLen);

    processName = basename(buf);

    delete[] buf;
    return processName;
}

// Background the process by detaching it from the console and redirecting
// std in, out, and err to /dev/null
int daemonizeService(bool closeStdStreams)
{
    int nochdir = 1;
    int noclose = closeStdStreams ? 0 : 1;
    int rc = daemon(nochdir, noclose);
    // Here after the fork; the parent is dead and setsid() is called
    if (rc != 0)
    {
        std::cerr << "Error: unable to daemonize: " << strerror(errno) << "\n";
    }
    return rc;
}

int daemonizeService(const char *stdOutFile, const char *stdErrFile)
{
    int rc = daemonizeService(false);

    if (rc != 0)
    {
        std::cerr << "Error: \n";
        return rc;
    }

    sysLogRedirect(stdOutFile, stdErrFile);

    return 0;
}

void sysLogRedirect(const char *stdOutFile, const char *stdErrFile)
{
    int rc = 0;

    // Redirect stdin to /dev/null
    int nullfd = open("/dev/null", O_RDONLY);
    dup2(nullfd, STDIN_FILENO);
    close(nullfd);

    // Get timestamp.
    struct timeval tv;
    rc = gettimeofday(&tv, NULL);
    if (rc != 0)
    {
        std::cerr << "Fail to get current time" << std::endl;
        tv.tv_sec = 0;
    }

    int stdErrFlag, stdOutFlag;
    //Always use append-write. And if not exist, create it.
    stdErrFlag = stdOutFlag = O_CREAT | O_APPEND | O_WRONLY;

    // Redirect stderr to stdErrFile
    struct stat st;
    rc = stat(stdErrFile, &st);
    if (rc != 0 || st.st_size > MAX_ERR_OUTPUT)
    {
        // file may not exist or oversize
        stdErrFlag |= O_TRUNC; // Remove old content if any.
    }

    int errfd = open(stdErrFile, stdErrFlag, RWRR);
    dup2(errfd, STDERR_FILENO);
    close(errfd);
    setvbuf(stderr, NULL, _IONBF, 0); // Make sure stderr is not buffering
    std::cerr << "Process " << getpid() << " built error output at "
            << tv.tv_sec << std::endl;

    // Redirect stdout to stdOutFile
    rc = stat(stdOutFile, &st);
    if (rc != 0 || st.st_size > MAX_STD_OUTPUT)
    {
        // file may not exist or oversize
        stdOutFlag |= O_TRUNC; // Remove old content if any.
    }

    int outfd = open(stdOutFile, stdOutFlag, RWRR);
    dup2(outfd, STDOUT_FILENO);
    close(outfd);
    setvbuf(stdout, NULL, _IONBF, 0); // Make sure stdout not buffering
    std::cout << "Process " << getpid() << " built standard output at "
            << tv.tv_sec << std::endl;

    return;
}

