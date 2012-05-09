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
 * traverseselectdir.cpp
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */


#include "io/rollselectdir.h"
#include "io/io.h"
#include "trace/log.h"

void CRollSelectDir::setBaseDir(std::string baseDir)
{
    mBaseDir = baseDir;

    std::vector<std::string> dirList;
    int rc = getDirList(dirList, mBaseDir, "");
    if (rc)
    {
        LOG_ERROR("Failed to all subdir entry");
    }

    if (dirList.size() == 0)
    {
        MUTEX_LOCK(&mMutex);

        mSubdirs.clear();
        mSubdirs.push_back(mBaseDir);
        mPos = 0;
        MUTEX_UNLOCK(&mMutex);

        return ;
    }


    MUTEX_LOCK(&mMutex);
    mSubdirs = dirList;
    mPos = 0;
    MUTEX_UNLOCK(&mMutex);
    return ;
}

std::string CRollSelectDir::select()
{
    std::string ret;

    MUTEX_LOCK(&mMutex);
    ret = mSubdirs[mPos % mSubdirs.size()];
    mPos++;
    MUTEX_UNLOCK(&mMutex);

    return ret;

}
