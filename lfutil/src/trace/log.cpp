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
 * @ func:  provide project common log functions
 */


#include <stdio.h>
#include <assert.h>
#include <exception>
#include <stdarg.h>

#include "trace/log.h"
#include "lang/lstring.h"
#include "io/io.h"


CLog::CLog(const std::string &logFileName, 
           const LOG_LEVEL    logLevel,
           const LOG_LEVEL    consoleLevel ):
    mLogName(logFileName),
    mLogLevel(logLevel),
    mConsoleLevel(consoleLevel)
{
    mPrefixMap[LOG_LEVEL_PANIC] = "PANIC:";
    mPrefixMap[LOG_LEVEL_ERR]   = "ERROR:";
    mPrefixMap[LOG_LEVEL_WARN]  = "WARNNING:";
    mPrefixMap[LOG_LEVEL_INFO]  = "INFO:";
    mPrefixMap[LOG_LEVEL_DEBUG] = "DEBUG:";
    mPrefixMap[LOG_LEVEL_TRACE] = "TRACE:";

    pthread_mutex_init(&mLock, NULL);

    mLogDate.mYear = -1;
    mLogDate.mMon  = -1;
    mLogDate.mDay  = -1;
    mLogMaxLine    = LOG_MAX_LINE;
    mLogLine       = -1;
    mRotateType    = LOG_ROTATE_BYDAY;
    
    CheckParamValid();

}

CLog::~CLog(void)
{
    pthread_mutex_lock(&mLock);
    if (mOfs.is_open())
    {
        mOfs.close();
    }
    pthread_mutex_unlock(&mLock);

    pthread_mutex_destroy(&mLock);
}

void
CLog::CheckParamValid()
{
    assert( !mLogName.empty() );
    int creatable = touch(mLogName);
    assert(creatable == 0);
    assert( LOG_LEVEL_PANIC <= mLogLevel && mLogLevel < LOG_LEVEL_LAST );
    assert( LOG_LEVEL_PANIC <= mConsoleLevel && mConsoleLevel < LOG_LEVEL_LAST );
    
    return ;
}

int 
CLog::Output( const LOG_LEVEL level, const char *module, const char *prefix, const char *f, ...  )
{
    bool   locked = false;
    try
    {
        va_list args;
        char    msg[ONE_KILO] = {0};

        va_start(args, f);
        vsnprintf(msg, sizeof(msg), f, args);
        va_end(args);

        if ( LOG_LEVEL_PANIC <= level && level <= mConsoleLevel )
        {
            std::cout << msg << std::endl;
        }
        else if (mDefaultSet.find(module) != mDefaultSet.end())
        {
            std::cout << msg << std::endl;
        }

        if ( LOG_LEVEL_PANIC <= level && level <= mLogLevel )
        {
            pthread_mutex_lock(&mLock);
            locked = true;
            mOfs << prefix;
            mOfs << msg;
            mOfs << "\n";
            mOfs.flush();
            mLogLine++;
            pthread_mutex_unlock(&mLock);
            locked = false;
        }
        else if (mDefaultSet.find(module) != mDefaultSet.end())
        {
            pthread_mutex_lock(&mLock);
            locked = true;
            mOfs << prefix;
            mOfs << msg;
            mOfs << "\n";
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

int 
CLog::SetConsoleLevel( LOG_LEVEL consoleLevel )
{
    if ( LOG_LEVEL_PANIC <= consoleLevel && consoleLevel < LOG_LEVEL_LAST )
    {
        mConsoleLevel = consoleLevel;
        return LOG_STATUS_OK;
    }
    
    return LOG_STATUS_ERR;
}
LOG_LEVEL
CLog::GetConsoleLevel()
{
    return mConsoleLevel;
}

int 
CLog::SetLogLevel( LOG_LEVEL logLevel )
{
    if ( LOG_LEVEL_PANIC <= logLevel && logLevel < LOG_LEVEL_LAST )
    {
        mLogLevel = logLevel;
        return LOG_STATUS_OK;
    }

    return LOG_STATUS_ERR;
}

LOG_LEVEL
CLog::GetLogLevel()
{
    return mLogLevel;
}

const char *
CLog::PrefixMsg(LOG_LEVEL level)
{
    if ( LOG_LEVEL_PANIC <= level && level < LOG_LEVEL_LAST )
    {
        return mPrefixMap[level].c_str();
    }
    static const char *emptyPrefix="";
    return emptyPrefix;
}

void
CLog::SetDefaultModule(const std::string &modules)
{
    CLstring::splitString( modules, ",", mDefaultSet);
}

int 
CLog::SetRotateType(LOG_ROTATE rotateType)
{
    if (LOG_ROTATE_BYDAY <= rotateType && rotateType < LOG_ROTATE_LAST)
    {
        mRotateType = rotateType;
    }
    return LOG_STATUS_OK;
}

LOG_ROTATE 
CLog::GetRotateType()
{
    return mRotateType;
}

int
CLog::RotateByDay(const int year, const int month, const int day)
{
    if (mLogDate.mYear == year  &&
        mLogDate.mMon  == month &&
        mLogDate.mDay  == day )
    {
        //Don't need rotate
        return 0;
    }
    
    char date[16] = {0};
    snprintf(date, sizeof(date), "%04d%02d%02d", year, month, day);
    std::string logFileName = mLogName + "." + date;
    
    
    if (mOfs.is_open())
    {
        mOfs.close();
    }
    mOfs.open(logFileName.c_str(), std::ios_base::out|std::ios_base::app);
    if (mOfs.good())
    {
        mLogDate.mYear = year;
        mLogDate.mMon  = month;
        mLogDate.mDay  = day;
        
        mLogLine = 0;
    }
    

    return 0;
}

int
CLog::RenameOldLogs()
{
    int logIndex    = 1;
    int maxLogIndex = 0;
    char logIndexStr[4] = {0};
    
    while(logIndex < 999)
    {
        snprintf(logIndexStr, sizeof(logIndexStr), "%03d", logIndex);
        
        std::string logName = mLogName + "." + logIndexStr;
        int result = access(logName.c_str(), R_OK);
        if (result)
        {
            break;
        }

        maxLogIndex = logIndex;
        logIndex++;
    }

    logIndex = maxLogIndex;
    while(logIndex > 0)
    {
        snprintf(logIndexStr, sizeof(logIndexStr), "%03d", logIndex);
        
        std::string logNameOld = mLogName + "." + logIndexStr;

        snprintf(logIndexStr, sizeof(logIndexStr), "%03d", logIndex + 1);
        
        std::string logNameNew = mLogName + "." + logIndexStr;

        int result = rename(logNameOld.c_str(), logNameNew.c_str());
        if ( result )
        {
            return LOG_STATUS_ERR;
        }
        logIndex--;
    }

    return LOG_STATUS_OK;
}

int
CLog::RotateBySize()
{
    if (mLogLine < 0)
    {
        //The first time open log file
        mOfs.open(mLogName.c_str(), std::ios_base::out|std::ios_base::app);
        mLogLine = 0;
        return LOG_STATUS_OK;
    }
    else if (0 <= mLogLine && mLogLine < mLogMaxLine)
    {
        //Don't need rotate
        return LOG_STATUS_OK;
    }
    else 
    {
        
        int result = RenameOldLogs();
        if (result)
        {
            //rename old logs failed
            return LOG_STATUS_OK;
        }

        if (mOfs.is_open())
        {
            mOfs.close();
        }

        char logIndexStr[4] = {0};
        snprintf(logIndexStr, sizeof(logIndexStr), "%03d", 1);
        std::string logNameNew = mLogName + "." + logIndexStr;
        result = rename(mLogName.c_str(), logNameNew.c_str());
        if ( result )
        {
            std::cerr << "Failed to rename " << mLogName << " to " <<  logNameNew << std::endl;
        }
        
        mOfs.open(mLogName.c_str(), std::ios_base::out|std::ios_base::app);
        if (mOfs.good())
        {
            mLogLine = 0;
        }
        else
        {
            //Error
            mLogLine = mLogMaxLine;
        }
        
        return LOG_STATUS_OK;
    }

    return LOG_STATUS_OK;
}

int 
CLog::Rotate(const int year, const int month, const int day)
{
    int result = 0;
    pthread_mutex_lock(&mLock);
    if (mRotateType == LOG_ROTATE_BYDAY)
    {
        result = RotateByDay(year, month, day);
    }
    else
    {
        result = RotateBySize();
    }
    pthread_mutex_unlock(&mLock);

    return result;
}

