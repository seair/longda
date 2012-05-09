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
 * traverseselectdir.h
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */

#ifndef TRAVERSESELECTDIR_H_
#define TRAVERSESELECTDIR_H_

#include <map>
#include <string>
#include <vector>

#include "defs.h"
#include "os/mutex.h"
#include "io/selectdir.h"

class CRollSelectDir : public CSelectDir
{
public:

    CRollSelectDir(){MUTEX_INIT(&mMutex, NULL);}
    ~CRollSelectDir(){MUTEX_DESTROY(&mMutex);}

public:
    /**
     * inherit from CSelectDir
     */
    std::string select();
    void setBaseDir(std::string baseDir);


public:
    std::string                 mBaseDir;
    std::vector<std::string>    mSubdirs;
    pthread_mutex_t             mMutex;
    u32_t                       mPos;

};


#endif /* TRAVERSESELECTDIR_H_ */
