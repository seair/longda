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
 * ini.h
 * This file read ini configuration file
 *  Created on: 2012-3-6
 *      Author: Longda Feng
 */

#if !defined(__LFUTIL_INI_H__)
#define __LFUTIL_INI_H__

#include <string>
#include <map>
#include <set>
#include <iostream>
#include <stdio.h>

//********************************************************************
//#means comments
//Ini configuration format
//[section]
//VARNAME=VALUE



class CIni
{
public:
    /**
     * To simplify the logic, no lock's when loading configuration
     * So don't modify the data parallel
     */
    CIni();
    ~CIni();

    /**
     * load one ini configuration
     * it support load multiple ini configuration files
     * @return, 0 means success, others means failed
     */
    int load(const std::string &iniFile);

    /**
     * get the map of the section
     * if the section doesn't exist, return one empty section
     */
    const std::map<std::string, std::string>& get(const std::string &section =
            DEFAULT_SECTION);

    /**
     * get the value of the key in the section,
     * if the key-value doesn't exist,
     * use the input defaultValue
     */
    std::string get(const std::string &key, const std::string &defaultValue,
            const std::string &section = DEFAULT_SECTION);

    /**
     * put the key-value pair to the section
     * if the key-value already exist, just replace it
     * if the section doesn't exist, it will create this section
     */
    int put(const std::string &key, const std::string &value,
            const std::string &section = DEFAULT_SECTION);

    /**
     * output all configuration to one string
     */
    void output(std::string &outputStr);

    static const std::string DEFAULT_SECTION;

    // one line max length
    static const int MAX_CFG_LINE_LEN = 1024;

    // value split tag
    static const char CFG_DELIMIT_TAG = ',';

    // comments's tag
    static const char CFG_COMMENT_TAG = '#';

    // continue line tag
    static const char CFG_CONTINUE_TAG = '\\';

    // session name tag
    static const char CFG_SESSION_START_TAG = '[';
    static const char CFG_SESSION_END_TAG   = ']';

protected:
    /**
     * insert one empty session to mSections
     */
    void insertSession(const std::string &sessionName);

    /**
     * switch session according to the sessionName
     * if the section doesn't exist, it will create one
     */
    std::map<std::string, std::string>* switchSession(const std::string &sessionName);

    /**
     * insert one entry to sessionMap
     * line's format is "key=value"
     *
     */
    int insertEntry(std::map<std::string, std::string>* sessionMap, const std::string &line);


    typedef std::map<std::string, std::map<std::string, std::string> > SessionsMap;
private:

    static const std::map<std::string, std::string>    mEmptyMap;

    std::set<std::string> mFileNames;
    SessionsMap           mSections;
};

/**
 * Global configurate propertis
 */
CIni*& theGlobalProperties();
//********************************************************************

#endif //__LFUTIL_INI_H__
