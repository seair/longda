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
 * @ func:  provide project common string operation
 */

#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <errno.h>
#include <string.h>
#include <iomanip>
#include <algorithm>
#include <ctype.h>

#include "lang/lstring.h"

char* CLstring::strip(char* str_)
{
    if (str_ == NULL || *str_ == 0)
        return str_;

    char* head = str_;
    while (isspace(*head))
        ++head;

    char* last = str_ + strlen(str_) - 1;
    while (isspace(*last) && last != str_)
        --last;
    *(last + 1) = 0;
    return head;
}

void CLstring::strip(std::string & str)
{
    size_t head = 0;

    while (isspace(str[head]))
    {
        ++head;
    }

    size_t tail = str.size() - 1;
    while (isspace(str[tail]) && tail != head)
    {
        --tail;
    }

    str = str.substr(head, (tail - head) + 1);
}

// Translation functions with templates are defined in the header file
std::string CLstring::sizeToPadStr(int size, int pad)
{
    std::ostringstream ss;
    ss << std::setw(pad) << std::setfill('0') << size;
    return ss.str();
}

std::string& CLstring::strToUpper(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), (int (*)(int)) &std::toupper);
    return s;
}

std::string& CLstring::strToLower(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), (int (*)(int)) &std::tolower);
    return s;
}

void CLstring::splitString(const std::string &str, std::string delim,
        std::set<std::string>& results)
{
    int cutAt;
    std::string tmpStr(str);
    while ((cutAt = tmpStr.find_first_of(delim)) != (signed) tmpStr.npos)
    {
        if (cutAt > 0)
        {
            results.insert(tmpStr.substr(0, cutAt));
        }
        tmpStr = tmpStr.substr(cutAt + 1);
    }

    if (tmpStr.length() > 0)
    {
        results.insert(tmpStr);
    }
}

void CLstring::splitString(const std::string &str, std::string delim,
        std::vector<std::string>& results)
{
    int cutAt;
    std::string tmpStr(str);
    while ((cutAt = tmpStr.find_first_of(delim)) != (signed) tmpStr.npos)
    {
        if (cutAt > 0)
        {
            results.push_back(tmpStr.substr(0, cutAt));
        }
        tmpStr = tmpStr.substr(cutAt + 1);
    }

    if (tmpStr.length() > 0)
    {
        results.push_back(tmpStr);
    }
}

void CLstring::splitString(char* str, char dim, std::vector<char *> &results,
        bool keep_null)
{
    char* p = str;
    char* l = p;
    while (*p)
    {
        if (*p == dim)
        {
            *p++ = 0;
            if (p - l > 1 || keep_null)
                results.push_back(l);
            l = p;
        }
        else
            ++p;
    }
    if (p - l > 0 || keep_null)
        results.push_back(l);
    return;
}


void CLstring::replace(std::string &str, const std::string &old, const std::string &newStr)
{
    if (old.compare(newStr) == 0 )
    {
        return ;
    }

    if (old == "")
    {
        return ;
    }

    if (old.length() > str.length())
    {
        return ;
    }

    std::string result;

    size_t index ;
    size_t lastIndex = 0;

    while((index = str.find(old, lastIndex)) != std::string::npos)
    {
        result += str.substr(lastIndex, index - lastIndex);
        result += newStr;
        lastIndex = index + old.length();
    }

    result += str.substr(lastIndex, str.length() - lastIndex + 1);

    str = result;

    return ;
}



char *CLstring::bin2hex(const char *s, const int len, char *szHexBuff)
{
    int nLen = 0;
    unsigned char *pEnd = (unsigned char *)s + len;
    for (unsigned char * p=(unsigned char *)s; p < pEnd; p++)
    {
        nLen += sprintf(szHexBuff + nLen, "%02x", *p);
    }

    szHexBuff[nLen] = '\0';
    return szHexBuff;
}

char *CLstring::hex2bin(const char *s, char *szBinBuff, int *nDestLen)
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
