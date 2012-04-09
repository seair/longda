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
 * regex.cpp
 *
 *  Created on: Mar 26, 2012
 *      Author: Longda Feng
 */

#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

#include "math/lregex.h"

int regex_match(const char* str_, const char* pat_)
{
    regex_t reg;
    if(regcomp(&reg, pat_, REG_EXTENDED|REG_NOSUB))
        return -1;

    int ret = regexec (&reg, str_, 0, NULL, 0);
    regfree (&reg);
    return ret;
}
