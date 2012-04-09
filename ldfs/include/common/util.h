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


/*
 * author: hustjackie@Longda.com
 * date:   2011/04/01
 * func:   Provide report enter entry
 */ 


#ifndef __UTIL_H__
#define __UTIL_H__

// Basic includes
#include <signal.h>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <assert.h>
#include <vector>

#include "defs.h"
#include "xlateutil.h"
#include "cfgreader.h"

class ConfigPath{
public:
    ConfigPath(){}
    void Init(std::string processName)
    {
        assert(processName.empty() == false);
        mProcessName = processName;
        if (mStdOut.empty() == true )
        {
            mStdOut = mProcessName + ".out";
        }
        if (mStdErr.empty() == true )
        {
            mStdErr = mProcessName + ".err";
        }
        if (mProperties.empty() == true)
        {
            mProperties= "./etc/" + mProcessName + ".cfg";
        }

        mDemon = false;

        return ;
    }
    
public:
    std::string mStdOut;       //! The output file
    std::string mStdErr;       //! The err output file
    std::string mProperties;   //! The properties config file
    std::string mProcessName;  //! The process name
    bool        mDemon;        // whether demon or not
};

//! Global config files path
ConfigPath*& GlobalConfigPath();

//! Global configurate propertis
CCfgReader*& theGlobalProperties();

int  UtilInit(ConfigPath *pConfigFiles);
void UtilFinalize();

int InitLog(const std::string &procName, CCfgReader *procProperties);
void CleanupLog();


//! Get process Name
/**
 * @param[in]   progFullName  process full name with full path  
 * @return      processName   process name without directory path
 */
std::string GetProcessName(const char *progFullName);
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
int DaemonizeService(const char *stdOutFile, const char *stdErrFile);

void SysLogRedirect(const char *stdOutFile, const char *stdErrFile);

//! Default function that blocks signals.
/**
 * Now it blocks SIGINT, SIGTERM, and SIGUSR1 
 */
void BlockSignalsDefault(sigset_t *signal_set, sigset_t *old_set);
//! Default function that unblocks signals.
/**
 * It unblocks SIGINT, SIGTERM,and SIGUSR1.
 */
void UnBlockSignalsDefault(sigset_t *signal_set, sigset_t *old_set);
    
void WaitForSignals(sigset_t *signal_set, int& sig_number);

// Set signal handling function
/**
 * handler function
 */
typedef void (*sighandler_t)(int);
void SetSignalHandlingFunc(sighandler_t func);
void SetSigFunc(int sig, sighandler_t func);

int ReadFromFile(const  std::string &fileName, std::string &data);
int GetFileLines(const  std::string &fileName, u64_t       &lineNum);

//! Get file list from the dir
/**
 *
 * @param[out]  fileList   file List 
 * @param[in]   path       the search path
 * @param[in]   pattern    regex string, if not empty, the file should match list
 * @param[in]   resursion  if this has been set, it will search subdirs
 * @return  0   if success, error code otherwise
 */
int GetFileList(std::vector<std::string> &fileList, const std::string &path, const std::string &pattern, bool resusion);
int GetFileNum(u64_t &fileNum, const std::string &path, const std::string &pattern, bool resusion);


int regex_match(const char* str_, const char* pat_);

int GetHostname(const char *ip, std::string& hostname);

char *Bin2Hex(const char *s, const int len, char *szHexBuff);
char *Hex2Bin(const char *s, char *szBinBuff, int *nDestLen);

void GetDirName(const char *path, std::string &parent);
void GetFileName(const char *path, std::string &fileName);

long int Random(const long int scope);

#endif // __UTIL_H__
