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


/*
 * author: hustjackie@gmail.com
 * date:   2011/06/115
 * func:   meta data cache layer
 */ 

#ifndef __CMETADATA_H__
#define __CMETADATA_H__

#include <map>
#include <string>
#include <iostream>

#include "mutex.h"

#define DEFAULT_FREE_RATIO      0.8

typedef enum _metaOp
{
    META_READ      = 0,
    META_WRITE,
}MetaOp;

class CMetaCacheValue
{
public:
    CMetaCacheValue();
        
    CMetaCacheValue(char *data, int dataLen);

    ~CMetaCacheValue();

    bool Empty() {return mCounter == 0;}
    bool CanFree() {return (mCounter == 0 && 
        (mDeleted == true || mInconsistent == true || mForceFree == true)  );}

    int   Lock(MetaOp op);
    void  Unlock(MetaOp op);
public:
    std::auto_ptr<char> mData;
    int                 mDataLen;
    
    int                 mCounter;
    bool                mDeleted;
    bool                mInconsistent;
    bool                mForceFree;

    pthread_mutex_t     mLock;
    pthread_cond_t      mCond;
    bool                mLocking;
};

/*
 * use LRU Map cache arithmetic
 */
class CMetaCache
{
private:
    typedef std::map<std::string, CMetaCacheValue *>  base_type;
    typedef std::pair<std::string, CMetaCacheValue *> pair_type;
    typedef base_type::iterator                       base_iter;
    
    typedef u64_t                                     timestamp;
    typedef std::map<std::string, timestamp>          K2T;
    typedef std::map<timestamp, std::string>          T2K;
    typedef T2K::iterator                             T2K_iter;

public:
    CMetaCache(const std::string &dhtKey, const u32_t capacity);
    ~CMetaCache();

    int Init();

    const std::string& GetDHTNameSpace();

    const std::string& GetDHTKey();


    /**
     * Operation MetaData
     */
    int  SaveKeyV(const char *objId, char * data, u32_t dataLen, int retry = 3);

    int  DelKeyV(const char *objId, int retry = 3);

    int  GetKeyV(const char *objId, char **value, int *pbuflen, MetaOp op = META_WRITE, int retry = 3);

    int  PutKeyV(const char *objId, bool forceFree = false, MetaOp op = META_WRITE);

    int  AllocKeyV(const char *objId, char **value, int dataLen);

private:

    int  InsertMeta(const std::string &metaKey, char *data, u32_t dataLen);

    void UpdateLRU(const std::string &metaKey);

    void FreeMeta();

    void FreeMetaItem();

    void Erase(const std::string& k);

private:
    base_type   mMap;  //map to store data (Key, Value) 
    K2T         mK2t; // from key to timestamp
    T2K         mT2k; // from timestamp to key

    timestamp   mTm;
    size_t      mCapacity;

    std::string mNameSpace;
    std::string mMetaDhtKey;

    pthread_mutex_t mLock;
    
};

CMetaCache*& GlobalMetaCache();

CMetaCache*& GlobalDGCache();

#endif //__CMETADATA_H__
