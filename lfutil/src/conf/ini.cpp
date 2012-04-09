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
 * CIni.h
 * This file read CIni configuration file
 *  Created on: 2012-3-6
 *      Author: Longda Feng
 */

#include <fstream>
#include <string.h>
#include <errno.h>

#include "defs.h"
#include "conf/ini.h"
#include "lang/lstring.h"
#include "trace/log.h"

const std::string                        CIni::DEFAULT_SECTION = std::string("");
const std::map<std::string, std::string> CIni::mEmptyMap;

CIni::CIni()
{
}

CIni::~CIni()
{

}

void CIni::insertSession(const std::string &sessionName)
{
    std::map<std::string, std::string> sessionMap;
    std::pair<std::string, std::map<std::string, std::string> > entry =
            std::pair<std::string, std::map<std::string, std::string> >(
                    sessionName, sessionMap);

    mSections.insert(entry);
}

std::map<std::string, std::string>* CIni::switchSession(const std::string &sessionName)
{
    SessionsMap::iterator it = mSections.find(sessionName);
    if (it != mSections.end())
    {
        return &it->second;
    }

    insertSession(sessionName);

    it = mSections.find(sessionName);
    if (it != mSections.end())
    {
        return &it->second;
    }

    //should never go this
    return NULL;
}


const std::map<std::string, std::string>& CIni::get(const std::string &section)
{
    SessionsMap::iterator it = mSections.find(section);
    if (it == mSections.end())
    {
        return mEmptyMap;
    }

    return it->second;
}

std::string CIni::get(const std::string &key, const std::string &defaultValue,
            const std::string &section)
{
    std::map<std::string, std::string> sectionMap = get(section);

    std::map<std::string, std::string>::iterator it = sectionMap.find(key);
    if (it == sectionMap.end())
    {
        return defaultValue;
    }

    return it->second;
}

int CIni::put(const std::string &key, const std::string &value,
            const std::string &section)
{
    std::map<std::string, std::string> *pSectionMap = switchSession(section);

    pSectionMap->insert(std::pair<std::string, std::string>(key, value));

    return 0;
}

int CIni::insertEntry(std::map<std::string, std::string>* sessionMap, const std::string &line)
{
    if (sessionMap == NULL)
    {
        std::cerr << __FILE__ << __FUNCTION__ << " session map is null" << std::endl;
        return -1;
    }
    size_t equalPos = line.find_first_of('=');
    if (equalPos == std::string::npos)
    {
        std::cerr << __FILE__ << __FUNCTION__ << "Invalid configuration line " << line << std::endl;
        return -1;
    }

    std::string key = line.substr(0, equalPos );
    std::string value = line.substr(equalPos + 1);


    CLstring::strip(key);
    CLstring::strip(value);

    sessionMap->insert(std::pair<std::string, std::string>(key, value));

    return 0;
}


int CIni::load(const std::string &fileName)
{
    std::ifstream ifs;

    try
    {

        bool       continueLastLine = false;

        std::map<std::string, std::string>  *currentSession = switchSession(DEFAULT_SECTION);

        char line[MAX_CFG_LINE_LEN];

        std::string lineEntry;

        ifs.open(fileName.c_str());
        while(ifs.good())
        {

            memset(line, 0, sizeof(line));

            ifs.getline(line, sizeof(line));

            char *readBuf = CLstring::strip(line);

            if(strlen(readBuf) == 0)
            {
                //empty line
                continue;
            }

            if (readBuf[0] == CFG_COMMENT_TAG)
            {
                //comments line
                continue;
            }

            if (readBuf[0] == CFG_SESSION_START_TAG &&
                readBuf[strlen(readBuf) - 1] == CFG_SESSION_END_TAG)
            {

                readBuf[strlen(readBuf) - 1] = '\0';
                std::string sessionName = std::string(readBuf + 1);

                currentSession = switchSession(sessionName);

                continue;
            }

            if (continueLastLine == false)
            {
                // don't need continue last line
                lineEntry = readBuf;
            }
            else
            {
                lineEntry += readBuf;
            }

            if (readBuf[strlen(readBuf) - 1] == CFG_CONTINUE_TAG)
            {
                // this line isn't finished, need continue
                continueLastLine = true;

                //remove the last character
                lineEntry[lineEntry.size() - 1] = '\0';
                continue;
            }
            else
            {
                continueLastLine = false;
                insertEntry(currentSession, lineEntry);
            }
        }
        ifs.close();

        mFileNames.insert(fileName);
        std::cout << "Successfully load " << fileName << std::endl;
    }
    catch(...)
    {
        if (ifs.is_open())
        {
            ifs.close();
        }
        std::cerr << "Failed to load " << fileName
                << SYS_OUTPUT_ERROR << std::endl;
        return -1;
    }

    return 0;
}


void CIni::output(std::string &outputStr)
{
    outputStr.clear();

    outputStr += "Begin dump configuration\n";

    for (SessionsMap::iterator it = mSections.begin(); it != mSections.end(); it++)
    {
        outputStr += it->first;
        outputStr += "\n";

        std::map<std::string, std::string> &sectionMap = it->second;

        for (std::map<std::string, std::string>::iterator subIt = sectionMap.begin();
                subIt != sectionMap.end(); subIt++)
        {
            outputStr += subIt->first;
            outputStr += "=";
            outputStr += subIt->second;
            outputStr += "\n";
        }
    }

    outputStr += "Finish dump configuration \n";

    return ;
}

//! Accessor function which wraps global properties object
CIni*& theGlobalProperties()
{
    static CIni *gProperties = new CIni();
    return gProperties;
}
