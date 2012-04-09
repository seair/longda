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
 * @ func:  provide project log operations
 */

#ifndef _CLOG_H_
#define _CLOG_H_

#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <string>
#include <assert.h>
#include <time.h>
#include <pthread.h>

#include "defs.h"
const int          LOG_STATUS_OK   = 0;
const int          LOG_STATUS_ERR  = 1;
const int          LOG_MAX_LINE    = 100000;

typedef enum{
    LOG_LEVEL_PANIC         = 0,
    LOG_LEVEL_ERR           = 1,
    LOG_LEVEL_WARN          = 2,
    LOG_LEVEL_INFO          = 3,
    LOG_LEVEL_DEBUG         = 4,
    LOG_LEVEL_TRACE         = 5,
    LOG_LEVEL_LAST
}LOG_LEVEL;

typedef enum{
        LOG_ROTATE_BYDAY         = 0,
        LOG_ROTATE_BYSIZE,
        LOG_ROTATE_LAST
}LOG_ROTATE;

class CLog
{
public:
    CLog(const std::string &logName,
         const LOG_LEVEL    logLevel = LOG_LEVEL_INFO,
         const LOG_LEVEL    consoleLevel = LOG_LEVEL_WARN );
    ~CLog(void);

    

    /**
     * These functions won't output header information such as __FUNCTION__,
     * The user should control these information
     * If the header information should be outputed
     * please use LOG_PANIC, LOG_ERROR ...
     */
    template <class T>
    CLog& operator <<(T message);
    
    template <class T>
    int Panic(T message);

    template <class T>
    int Error(T message);

    template <class T>
    int Warnning(T message);

    template <class T>
    int Info(T message);

    template <class T>
    int Debug(T message);

    template <class T>
    int Trace(T message);

    int Output(const LOG_LEVEL level, const char *module, const char *prefix, const char *f, ...  );
    
    int SetConsoleLevel( const LOG_LEVEL consoleLevel );
    LOG_LEVEL GetConsoleLevel();

    int SetLogLevel( const LOG_LEVEL logLevel );
    LOG_LEVEL GetLogLevel();

    int SetRotateType(LOG_ROTATE rotateType);
    LOG_ROTATE GetRotateType();

    const char * PrefixMsg(const LOG_LEVEL level);

    /**
     * Set Default Module list
     * if one module is default module, 
     * it will output whatever output level is lower than mLogLevel or not
     */
    void SetDefaultModule(const std::string &modules);
    bool CheckOutput(const LOG_LEVEL logLevel, const char *module);

    int Rotate(const int year = 0, const int month = 0, const int day = 0);

private:
    void CheckParamValid();

    
    int RotateBySize();
    int RenameOldLogs();
    int RotateByDay(const int year, const int month, const int day);
    
    template <class T>
    int Out(const LOG_LEVEL consoleLevel, 
            const LOG_LEVEL logLevel,
            T &message);
    

private:
    
    pthread_mutex_t mLock;
    std::ofstream   mOfs;
    std::string     mLogName;
    LOG_LEVEL       mLogLevel;
    LOG_LEVEL       mConsoleLevel; 

    typedef struct _LogDate{
        int         mYear;
        int         mMon;
        int         mDay;
    }LogDate;
    LogDate         mLogDate;
    int             mLogLine;
    int             mLogMaxLine;
    LOG_ROTATE      mRotateType;
    
    
    typedef std::map< LOG_LEVEL, std::string > LogPrefixMap;
    LogPrefixMap  mPrefixMap;

    typedef std::set< std::string > DefaultSet;
    DefaultSet        mDefaultSet;
    
};

extern CLog *gLog;

#if defined(LINUX)
#define LOG_HEAD(prefix, level)                                     \
if (gLog){                                                          \
    time_t now_time;                                                \
    time(&now_time);                                                \
    struct tm * p = localtime(&now_time);                           \
    char szHead[64] = {0};                                          \
    if (p)                                                          \
    {                                                               \
        sprintf(szHead,"%d-%d-%d %d:%d:%u pid:%u tid:%u ",          \
            p->tm_year+1900, p->tm_mon+1, p->tm_mday,               \
            p->tm_hour, p->tm_min, p->tm_sec,                       \
            (u32_t)getpid(), (u32_t)gettid());                      \
        gLog->Rotate( p->tm_year+1900, p->tm_mon+1, p->tm_mday);    \
    }                                                               \
    snprintf(prefix, sizeof(prefix), "[%s %s %s %u %s]>>",          \
        szHead, __FILE__, __FUNCTION__, (u32_t)__LINE__,            \
        (gLog)->PrefixMsg(level));                                  \
}
#else
#define LOG_HEAD(prefix, level)                                     \
if (gLog){                                                          \
    time_t now_time;                                                \
    time(&now_time);                                                \
    struct tm * p = localtime(&now_time);                           \
    char szHead[64] = {0};                                          \
    if (p)                                                          \
    {                                                               \
        sprintf(szHead,"%d-%d-%d %d:%d:%u ",                        \
            p->tm_year+1900, p->tm_mon+1, p->tm_mday,               \
            p->tm_hour, p->tm_min, p->tm_sec);                      \
        gLog->Rotate( p->tm_year+1900, p->tm_mon+1, p->tm_mday);    \
    }                                                               \
    snprintf(prefix, sizeof(prefix), "[%s %s %s %u %s]>>",          \
        szHead, __FILE__, __FUNCTION__, (u32_t)__LINE__,            \
        (gLog)->PrefixMsg(level));                                  \
}
#endif

#define LOG_OUTPUT(level, fmt, ...)                                 \
if (gLog && gLog->CheckOutput(level, __FILE__)){                    \
    char prefix[ONE_KILO] = {0};                                    \
    LOG_HEAD(prefix, level);                                        \
    gLog->Output(level, __FILE__, prefix, fmt, ## __VA_ARGS__);     \
}

#define LOG_DEFAULT(fmt, ...) LOG_OUTPUT(gLog->GetLogLevel(), fmt, ## __VA_ARGS__)
#define LOG_PANIC(fmt, ...)   LOG_OUTPUT(LOG_LEVEL_PANIC, fmt, ## __VA_ARGS__)
#define LOG_ERROR(fmt, ...)   LOG_OUTPUT(LOG_LEVEL_ERR, fmt, ## __VA_ARGS__)
#define LOG_WARN(fmt, ...)    LOG_OUTPUT(LOG_LEVEL_WARN, fmt, ## __VA_ARGS__)
#define LOG_INFO(fmt, ...)    LOG_OUTPUT(LOG_LEVEL_INFO, fmt, ## __VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   LOG_OUTPUT(LOG_LEVEL_DEBUG, fmt, ## __VA_ARGS__)
#define LOG_TRACE(fmt, ...)   LOG_OUTPUT(LOG_LEVEL_TRACE, fmt, ## __VA_ARGS__)


template <class T> CLog&
CLog::operator <<(T msg)
{
    // at this time, the input level is the default log level
    Out(mConsoleLevel, mLogLevel, msg);
    return *this;
}

template <class T> 
int CLog::Panic(T message)
{
    return Out(LOG_LEVEL_PANIC, LOG_LEVEL_PANIC, message);
}

template <class T>
int CLog::Error(T message)
{
    return Out(LOG_LEVEL_ERR, LOG_LEVEL_ERR, message);
}

template <class T>
int CLog::Warnning(T message)
{
    return Out(LOG_LEVEL_WARN, LOG_LEVEL_WARN, message);
}

template <class T> 
int CLog::Info(T message)
{
    return Out(LOG_LEVEL_INFO, LOG_LEVEL_INFO, message);
}

template <class T> 
int CLog::Debug(T message)
{
    return Out(LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG, message);
}

template <class T> 
int CLog::Trace(T message)
{
    return Out(LOG_LEVEL_TRACE, LOG_LEVEL_TRACE, message);
}

template < class T > 
int CLog::Out(const LOG_LEVEL consoleLevel,
              const LOG_LEVEL logLevel,
              T               &msg)
{
    bool locked = false;
    try
    {
        char prefix[ONE_KILO] = {0};
        LOG_HEAD(prefix, logLevel);
        if ( LOG_LEVEL_PANIC <= consoleLevel && consoleLevel <= mConsoleLevel ) 
        {
            std::cout << mPrefixMap[consoleLevel] << msg;
        }

        if ( LOG_LEVEL_PANIC <= logLevel && logLevel <= mLogLevel ) 
        {
            pthread_mutex_lock(&mLock);
            locked = true;
            mOfs << prefix;
            mOfs << msg;
            mOfs.flush();
            mLogLine++;
            pthread_mutex_unlock(&mLock);
            locked = false;
        }
    }
    catch (std::exception &e)
    {
        if (locked)
        {
            pthread_mutex_unlock(&mLock);
        }
        std::cerr << e.what() << std::endl;
        return LOG_STATUS_ERR;
    }

    return LOG_STATUS_OK;
}


#ifndef ASSERT
#define ASSERT(expression, description, ...)                   \
do{                                                            \
    if( !(expression) )                                        \
    {                                                          \
        if ( gLog )                                            \
        {                                                      \
            LOG_PANIC(description, ## __VA_ARGS__);            \
            LOG_PANIC("\n");                                   \
        }                                                      \
        assert(expression);                                    \
    }                                                          \
}while(0)
#endif //ASSERT
#endif //_CLOG_H_

