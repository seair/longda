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
 * @ func:  provide project common init functions
 */


#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <signal.h>
#include <regex.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "util.h"

#define MAX_ERR_OUTPUT 100000    // 100k
#define MAX_STD_OUTPUT 100000    // 100k
#define RWRR (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

int
ReadFromFile(const  std::string &fileName, std::string &data)
{
    FILE *file = fopen( fileName.c_str(), "rb" );
    if ( file == NULL )
    {
        std::cerr << __FUNCTION__ << "Failed to open file " << fileName << std::endl;
        return -1;
    }
    
    //fseek( file, 0, SEEK_END );
    //size_t fsSize = ftell( file );
    //fseek( file, 0, SEEK_SET );
    
    char buffer[4 * ONE_KILO] = {0};
    size_t readSize  = 0;
    size_t oneRead   = 0;
    
    data.clear();
    do {
        memset(buffer, 0, sizeof(buffer));
        oneRead = fread( buffer, 1, sizeof(buffer), file );
        if (ferror(file))
        {
            std::cerr << __FUNCTION__ << "Failed to read data" << fileName << std::endl;
            fclose( file );
            return -1;
        }

        readSize += oneRead;
        data     += buffer;
        
    }while(feof(file) == 0);

    fclose( file );
    return 0;
}

int GetFileLines(const  std::string &fileName, u64_t       &lineNum)
{
    lineNum = 0;

    char line[4096] = {0};
    ifstream ifs ( fileName.c_str() );
    if (!ifs)
    {
        return -1;
    }

    while (ifs.good())
    {
        line[0] = 0;
        ifs.getline(line, sizeof(line));
        char *lineStrip = Xlate::strip(line);
        if (strlen(lineStrip))
        {
            lineNum++;
        }
    }

    ifs.close();
    return 0;
}


std::string GetProcessName(const char *progName)
{
    std::string processName;
    
    int   bufLen = strlen(progName);
    assert(bufLen);
    char *buf = new char [bufLen + 1];
    if (buf == NULL)
    {
        std::cerr << __FUNCTION__ 
            << ":Failed to alloc memory for program name." << std::endl;
        return "";
    }
    memset(buf, 0, bufLen + 1);
    strncpy(buf, progName, bufLen);

    processName = basename(buf);

    delete [] buf;
    return processName;
}

// Background the process by detaching it from the console and redirecting
// std in, out, and err to /dev/null
int DaemonizeService(bool closeStdStreams)
{
    int nochdir = 1;
    int noclose = closeStdStreams ? 0 : 1;
    int rc = daemon(nochdir, noclose);
    // Here after the fork; the parent is dead and setsid() is called
    if(rc != 0) {
        std::cerr << "Error: unable to daemonize: " << strerror(errno) << "\n";
    }
    return rc;
}

int DaemonizeService(const char *stdOutFile, const char *stdErrFile)
{
    int rc = DaemonizeService(false);

    if(rc != 0) {
        std::cerr << "Error: \n";
        return rc;
    }
    
    SysLogRedirect(stdOutFile, stdErrFile);
    return 0;
}

void SysLogRedirect(const char *stdOutFile, const char *stdErrFile)
{
    int rc = 0;
    
    // Redirect stdin to /dev/null
    int nullfd = open("/dev/null", O_RDONLY);
    dup2(nullfd, STDIN_FILENO);
    close(nullfd);       

    // Get timestamp.
    struct timeval tv;
    rc = gettimeofday(&tv, NULL);
    if(rc != 0) {
        std::cerr << "Fail to get current time" << std::endl;
        tv.tv_sec = 0;
    }

    int stdErrFlag, stdOutFlag;
    //Always use append-write. And if not exist, create it.
    stdErrFlag = stdOutFlag = O_CREAT|O_APPEND|O_WRONLY;  

    // Redirect stderr to stdErrFile 
    struct stat st;
    rc = stat(stdErrFile, &st);
    if(rc!= 0 || st.st_size > MAX_ERR_OUTPUT) {
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
    if(rc!=0 || st.st_size > MAX_STD_OUTPUT) {
        // file may not exist or oversize
        stdOutFlag |= O_TRUNC; // Remove old content if any.
    }
    
    int outfd = open(stdOutFile, stdOutFlag, RWRR); 
    dup2(outfd, STDOUT_FILENO);
    close(outfd);
    setvbuf(stdout, NULL, _IONBF, 0);  // Make sure stdout not buffering  
    std::cout << "Process " << getpid() << " built standard output at "
                  << tv.tv_sec << std::endl;

    return ;
}

void SetSigFunc(int sig, sighandler_t func)
{
    struct sigaction newsa, oldsa;
    sigemptyset(&newsa.sa_mask);
    newsa.sa_flags = 0;
    newsa.sa_handler = func;
    int rc = sigaction (sig, &newsa, &oldsa);
    if (rc) {
        std::cerr << "Failed to set signal "<< sig << std::endl;
    }
}

/*
** Set Singal handling Fucntion
*/
void SetSignalHandlingFunc(sighandler_t func)
{
    SetSigFunc(SIGQUIT, func);
    SetSigFunc(SIGHUP, func);
}

void BlockSignalsDefault(sigset_t *signal_set, sigset_t *old_set)
{
    sigemptyset(signal_set);
#ifndef DEBUG
    //SIGINT will effect our gdb debugging
    sigaddset(signal_set, SIGINT);
#endif
    sigaddset(signal_set, SIGTERM);
    sigaddset(signal_set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, signal_set, old_set);
}

void UnBlockSignalsDefault(sigset_t *signal_set, sigset_t *old_set)
{
    sigemptyset(signal_set);
#ifndef DEBUG
    sigaddset(signal_set, SIGINT);
#endif
    sigaddset(signal_set, SIGTERM);
    sigaddset(signal_set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, signal_set, old_set);
}

int regex_match(const char* str_, const char* pat_)
{
    regex_t reg;
    if(regcomp(&reg, pat_, REG_EXTENDED|REG_NOSUB))
        return -1;

    int ret = regexec (&reg, str_, 0, NULL, 0);
    regfree (&reg);
    return ret;
}

int GetFileList(std::vector<std::string> &fileList, const std::string &path, const std::string &pattern, bool resusion)
{    
    try
    {
        DIR* dirp = NULL;
        dirp = opendir(path.c_str());
        if( dirp == NULL )
        {
            return -1;
        }
        
        std::string    fullPath;
        struct dirent* entry = NULL;
        struct stat    fs;
        while( (entry = readdir(dirp)) != NULL )
        {
            //don't care ".", "..", ".****" hidden files  
            if( !strncmp(entry->d_name, ".", 1) )
            {
                continue;
            }

            fullPath  = path;
            if (path[path.size() - 1] != '/')
            {
                fullPath += '/';
            }
            fullPath += entry->d_name;
            if(stat(fullPath.c_str(), &fs) < 0)
            {
                continue;
            }
            
            if( fs.st_mode & S_IFDIR )
            {
                if(resusion == 0)
                {
                    continue;
                }

                if( GetFileList( fileList, fullPath, pattern, resusion ) < 0 )
                {
                    closedir(dirp); 
                    return -1;
                }
            }
            
            if(!(fs.st_mode & S_IFREG) )
            {
                //����ͨ�ļ�
                continue;
            }
            
            if( pattern.empty() == false && regex_match(entry->d_name, pattern.c_str()) )
            {
                //Don't match
                continue;
            }
    
            fileList.push_back(fullPath);
        }
    
        closedir(dirp);     
        return 0;
    }
    catch(...)
    {
    }
    return -1;
}

int GetFileNum(u64_t &fileNum, const std::string &path, const std::string &pattern, bool resusion)
{    
    try
    {
        DIR* dirp = NULL;
        dirp = opendir(path.c_str());
        if( dirp == NULL )
        {
            return -1;
        }
        
        std::string    fullPath;
        struct dirent* entry = NULL;
        struct stat    fs;
        while( (entry = readdir(dirp)) != NULL )
        {
            //don't care ".", "..", ".****" hidden files  
            if( !strncmp(entry->d_name, ".", 1) )
            {
                continue;
            }

            fullPath  = path;
            if (path[path.size() - 1] != '/')
            {
                fullPath += '/';
            }
            fullPath += entry->d_name;
            if(stat(fullPath.c_str(), &fs) < 0)
            {
                continue;
            }
            
            if( fs.st_mode & S_IFDIR )
            {
                if(resusion == 0)
                {
                    continue;
                }

                if( GetFileNum( fileNum, fullPath, pattern, resusion ) < 0 )
                {
                    closedir(dirp);
                    return -1;
                }
            }
            
            if(!(fs.st_mode & S_IFREG) )
            {
                //����ͨ�ļ�
                continue;
            }
            
            if( pattern.empty() == false && regex_match(entry->d_name, pattern.c_str()) )
            {
                //Don't match
                continue;
            }
    
            fileNum++;
        }
    
        closedir(dirp);     
        return 0;
    }
    catch(...)
    {
    }
    return -1;
}

int GetHostname(const char *ip, std::string& hostname)
{
    if (ip == NULL)
    {
        char name[256] = {0};
        if (gethostname(name, sizeof(name)) != 0)
        { 
            std::cerr << "Failed to get local hostname" << std::endl;
            return -1;
        } 
        else 
        {
            hostname = name;
            return 0;
        }
    }
    else
    {
        int rc;
        struct in_addr addr;
        rc = inet_aton(ip, &addr);
        if (rc == 0)
        {
            std::cerr << "Failed to get "<< ip << "  hostname " <<std::endl;
            return -1;
        }
        
        struct hostent hostent_struct, *record = NULL;
        char buff[4096];
        int err;
        rc =  gethostbyaddr_r(&addr, sizeof(addr), AF_INET,
               &hostent_struct, buff, sizeof(buff),
               &record, &err);
        if (rc)
        {
            std::cerr << "Failed to get "<< ip << "  hostname " <<std::endl;
            return 0;
        }

        hostname = hostent_struct.h_name;
        return 0;
    }
    
    return -1;
}

char *Bin2Hex(const char *s, const int len, char *szHexBuff)
{
    unsigned char *p;
    unsigned char *pEnd;
    int nLen;

    nLen = 0;
    pEnd = (unsigned char *)s + len;
    for (p=(unsigned char *)s; p<pEnd; p++)
    {
        nLen += sprintf(szHexBuff + nLen, "%02x", *p);
    }

    szHexBuff[nLen] = '\0';
    return szHexBuff;
}

char *Hex2Bin(const char *s, char *szBinBuff, int *nDestLen)
{
    char buff[3];
    char *pSrc;
    int nSrcLen;
    char *pDest;
    char *pDestEnd;

    nSrcLen = strlen(s);
    if (nSrcLen == 0)
    {
        *nDestLen = 0;
        szBinBuff[0] = '\0';
        return szBinBuff;
    }

    *nDestLen = nSrcLen / 2;
    pSrc = (char *)s;
    buff[2] = '\0';

    pDestEnd = szBinBuff + (*nDestLen);
    for (pDest=szBinBuff; pDest<pDestEnd; pDest++)
    {
        buff[0] = *pSrc++;
        buff[1] = *pSrc++;
        *pDest = (char)strtol(buff, NULL, 16);
    }

    *pDest = '\0';
    return szBinBuff;
}

void GetDirName(const char *path, std::string &parent)
{
    //Don't care the last character as '/'
    const char *endPos = strrchr(path, '/');
    if (endPos == NULL)
    {
        parent = path;
        return ;
    }

    if (endPos == path)
    {
        parent.assign(path, 1);
    }
    else
    {
        parent.assign(path, endPos - path);
    }
    
    return ;
}

void GetFileName(const char *path, std::string &fileName)
{
    //Don't care the last character as '/'
    const char *endPos = strrchr(path, '/');
    if (endPos == NULL)
    {
        fileName = path;
        return ;
    }

    if (strcmp(path, "/") == 0)
    {
        fileName.assign(path);;
    }
    else
    {
        fileName.assign(endPos + 1);
    }

    return ;
}

long int Random(const long int scope)
{
    srandom(time(0));
    long int ret = 0;
    long int randNum = 0;
    if (scope < (RAND_MAX / 10))
    {
        // if RAND_MAX is bigger RAND_MAX/10, the precision is not good
        randNum = random();
        ret = randNum % scope;
    }
    else
    {
        randNum = (long int)((((double)scope)/RAND_MAX) * random());
        //due to convert scope/RAND_MAX to double, it will lose precision, 
        // randNum is possibly bigger scope
        ret = randNum % scope;
    }
    
    return ret;
}
