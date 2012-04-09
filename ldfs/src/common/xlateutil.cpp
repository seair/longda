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

#include "xlateutil.h"


// Translation functions with templates are defined in the header file

std::string
Xlate::sizeToPadStr(int size, int pad)
{
    std::ostringstream ss;
    ss << std::setw(pad) << std::setfill('0') << size;
    return ss.str();
}

std::string&
Xlate::strToUpper(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   (int(*)(int)) &std::toupper);
    return s;
}

std::string&
Xlate::strToLower(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   (int(*)(int)) &std::tolower);
    return s;
}

void
Xlate::SplitString(const std::string &str, std::string delim, std::set<std::string>& results)
{
    int cutAt;
    std::string tmpStr(str);
    while((cutAt = tmpStr.find_first_of(delim)) != (signed)tmpStr.npos )
    {
        if(cutAt > 0)
        {
            results.insert(tmpStr.substr(0,cutAt));
        }
        tmpStr = tmpStr.substr(cutAt+1);
    }

    if(tmpStr.length() > 0)
    {
        results.insert(tmpStr);
    }
}

void
Xlate::SplitString(const std::string &str, std::string delim, std::vector<std::string>& results)
{
    int cutAt;
    std::string tmpStr(str);
    while((cutAt = tmpStr.find_first_of(delim)) != (signed)tmpStr.npos )
    {
        if(cutAt > 0)
        {
            results.push_back(tmpStr.substr(0,cutAt));
        }
        tmpStr = tmpStr.substr(cutAt+1);
    }

    if(tmpStr.length() > 0)
    {
        results.push_back(tmpStr);
    }
}

void
Xlate::SplitString(char* str, char dim, std::vector<char *> &results, bool keep_null )
{
    char* p = str;
    char* l = p;
    while(*p)
    {
        if( *p == dim )
        {
            *p++ = 0;
            if( p-l > 1 || keep_null )
                results.push_back( l );
            l = p;
        }
        else
            ++p;
    }
    if( p-l > 0 || keep_null )
        results.push_back( l );
    return ;
}

std::string Xlate::ExtFileName(const std::string &fullPath)
{
    std::string szRt;
    int pos;
    try
    {
        pos = fullPath.rfind("/"); 
        if(pos != std::string::npos && pos < fullPath.size()-1)
        {
            szRt = fullPath.substr(pos+1, fullPath.size() - pos - 1);
        }
        else if(pos == std::string::npos)
        {
            szRt = fullPath;
        }
        else
        {
            szRt = "";
        }
        
    }
    catch(...)
    {
    }
    return szRt;
}

std::string Xlate::ExtFilePath(const std::string &fullPath)
{
    std::string szRt;
    int pos;
    try
    {
        pos = fullPath.rfind("/"); 
        if(pos!= std::string::npos)
        {
            szRt = fullPath.substr(0,pos);
        }
        else if(pos == std::string::npos)
        {
            szRt = fullPath;
        }
        else
        {
            szRt = "";
        }
        
    }
    catch(...)
    {
    }
    return szRt;
}

char* 
Xlate::strip(char* str_)
{
    if(str_ == NULL || *str_ == 0)
        return str_;

    char* head = str_;
    while(isspace(*head))
        ++head;

    char* last = str_ + strlen(str_) - 1;
    while(isspace(*last) && last != str_)
        --last;
    *(last + 1) = 0;
    return head;
}

