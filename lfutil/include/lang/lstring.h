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
 * @ func:  provide project common string utility functions
 */


#ifndef __LF_STRING_H__
#define __LF_STRING_H__

// Basic includes
#include <signal.h>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <vector>
#include <typeinfo>
#include <cstdlib>
#include <cxxabi.h>

#include <stdlib.h>

#include "defs.h"

namespace CLstring {

    /**
     * remove all white space(like ' ', '\t', '\n') from string
     */
    void strip(std::string & str);
    char* strip(char *str);

    //! Convert an integer size in a padded string
    /**
     * @param[in]   size    size to be converted and 0 padded
     * @param[in]   pad     decimal digits for the string padding
     * return   output 0-padded size string
     */
    std::string sizeToPadStr(int size, int pad);

    //! Convert a string to upper case.
    /**
     * @param[in,out] s the string to modify
     * @return a reference to the string that was modified.
     */
    std::string& strToUpper(std::string& s);

    //! Convert a string to lower case.
    /**
     * @param[in,out] s the string to modify
     * @return a reference to the string that was modified.
     */
    std::string& strToLower(std::string& s);

    //! Split string str using 'delimiter's
    /**
    * @param[in]      str        the string to be split up
    * @param[in]      delims     elimiter characters
    * @param[in,out] results     ector containing the split up string
    */
    void splitString(const std::string &str, std::string delim,
                     std::set<std::string>& results);
    void splitString(const std::string &str, std::string delim,
                     std::vector<std::string>& results);
    void splitString(char* str, char dim, std::vector<char *> &results, bool keep_null = false );


    /**
     * replace old with new in the string
     */
    void replace(std::string &str, const std::string &old, const std::string &newStr);

    /*
     * binary to hexadecimal
     */
    char *bin2hex(const char *s, const int len, char *szHexBuff);
    /*
     * hexadecimal to binary
     */
    char *hex2bin(const char *s, char *szBinBuff, int *nDestLen);



    //! Convert a number in a string format to a numeric value
    /**
     * @param[in]   str     input number string
     * @param[out]  val     output value
     * @param[in]   radix   an optional parameter that specifies the 
     *                      radix for the conversion.  By default, the
     *                      radix is set to std::dec (decimal).  See
     *                      also, std::oct (octal) and std::hex
     *                      (hexidecimal).
     * @return \c true if the string was successfully converted to a
     *         number, \c false otherwise
     */
    template<class T> bool strToVal(const std::string& str, T& val,
                      std::ios_base& (*radix)(std::ios_base&) = std::dec);

    //! Convert a numeric value into its string representation
    /**
     * @param[in]   val     numeric value
     * @param[out]  str     string representation of the numeric value
     * @param[in]   radix   an optional parameter that specifies the 
     *                      radix for the conversion.  By default, the
     *                      radix is set to std::dec (decimal).  See
     *                      also, std::oct (octal) and std::hex
     *                      (hexidecimal).
     */
    template<class T> void valToStr(const T& val, std::string& str,
                      std::ios_base& (*radix)(std::ios_base&) = std::dec);

    /**
     * get type's name
     */
    template<class T>std::string getTypeName(const T& val);
}

// Templatized functions have to be in the header file
template<class T> bool
CLstring::strToVal(const std::string& str, T& val,
                std::ios_base& (*radix)(std::ios_base&) /* = std::dec */)
{
    bool success = true;
    std::istringstream is(str);
    if (!(is >> radix >> val))
    {
        val = 0;
        success = false;
    }
    return success;
}

template<class T> void
CLstring::valToStr(const T& val, std::string& str,
                std::ios_base& (*radix)(std::ios_base&) /* = std::dec */)
{
    std::stringstream strm;
    strm << radix << val;
    str = strm.str();
}

template<class T>std::string 
CLstring::getTypeName(const T &val)
{
    int status = 0;
    char* stmp = abi::__cxa_demangle(typeid(val).name(), 0, 0, &status);
    if(!stmp)
        return "";

    std::string sret(stmp);

    ::free(stmp);
    return sret;
}

#endif // __LF_STRING_H__
