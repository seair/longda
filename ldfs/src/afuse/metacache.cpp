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
 * date:   2011/06/15
 * func:   meta data cache layer
 */

#include "log.h"
#include "util.h"
#include "md5.h"
#include "xlateutil.h"

#include "fdfs.h"
#include "fdht.h"
#include "metacache.h"
#include "metadata.h"

static const size_t DEFAULT_CAPACITY = ONE_KILO;

CMetaCacheValue::CMetaCacheValue() :
        mData(NULL), mDataLen(0), mCounter(0), mDeleted(false), mInconsistent(
                false), mForceFree(false), mLocking(false)
{
    MUTEX_INIT(&mLock, NULL);
    COND_INIT(&mCond, NULL);
}

CMetaCacheValue::CMetaCacheValue(char *data, int dataLen) :
        mData(data), mDataLen(dataLen), mCounter(0), mDeleted(false), mInconsistent(
                false), mForceFree(false), mLocking(false)
{
    MUTEX_INIT(&mLock, NULL);
    COND_INIT(&mCond, NULL);
}

CMetaCacheValue::~CMetaCacheValue()
{
    MUTEX_LOCK(&mLock);
    COND_DESTROY(&mCond);

    //don't need free memory due to auto_ptr
}

int CMetaCacheValue::Lock(MetaOp op)
{
    if (op == META_READ)
    {
        //Can't lock here 
        mCounter++;
        return STATUS_SUCCESS;
    } else if (op == META_WRITE)
    {
        MUTEX_LOCK(&mLock);
        while (mLocking)
        {
            COND_WAIT(&mCond, &mLock);
        }
        mLocking = true;
        mCounter++;
        return STATUS_SUCCESS;
    }

    return STATUS_UNKNOW_ERROR;

}

void CMetaCacheValue::Unlock(MetaOp op)
{
    if (op == META_READ)
    {
        mCounter--;
        return;
    } else if (op == META_WRITE)
    {
        mCounter--;
        mLocking = false;
        COND_SIGNAL(&mCond);
        MUTEX_UNLOCK(&mLock);
        return;
    }

    return;

}

CMetaCache::CMetaCache(const std::string &dhtKey, const u32_t capacity) :
        mMetaDhtKey(dhtKey), mCapacity(capacity)
{
    MUTEX_INIT(&mLock, NULL);
}

int CMetaCache::Init()
{
    const std::string nameSapceKey("NAMESPACE");
    mNameSpace = theGlobalProperties()->GetParamValue(nameSapceKey,
            DEFAULT_SESSION);
    if (mNameSpace.empty())
    {
        LOG_ERROR("Failed to get namespace");
        return STATUS_INIT_CACHE;
    } else if (mNameSpace.size() >= AFUSE_MAX_NAMESPACE_LEN)
    {
        LOG_ERROR("Namespace size :%s is bigger than %d",
                mNameSpace.c_str(), AFUSE_MAX_NAMESPACE_LEN);
        return STATUS_INIT_CACHE;
    }

    return STATUS_SUCCESS;
}

CMetaCache::~CMetaCache()
{
    //Skip lock in destruction function
    MUTEX_LOCK(&mLock);
    while (mMap.size())
    {
        base_iter it = mMap.begin();
        Erase(it->first);
    }MUTEX_UNLOCK(&mLock);

    MUTEX_DESTROY(&mLock);
}

int CMetaCache::SaveKeyV(const char *objId, char * data, u32_t dataLen,
        int retry)
{
    std::string metaKey(objId);

    int res = STATUS_SUCCESS;

    MUTEX_LOCK(&mLock);

    base_iter it = mMap.find(metaKey);

    MUTEX_UNLOCK(&mLock);

    if (it == mMap.end())
    {
        LOG_ERROR("Failed to find %s meta in meta cache, rc:%d:%s",
                objId, ENOENT, strerror(ENOENT));
        return ENOENT;
    } else
    {
        if (data != it->second->mData.get())
        {
            LOG_WARN(
                    "Save metadata %p is not equal as the address of metadata in metaMap:%p",
                    data, it->second->mData.get());
            it->second->mInconsistent = true;
        }
        if (dataLen != it->second->mDataLen)
        {
            LOG_WARN(
                    "Save metadata length %u is not equal as the length of metadata in metaMap %d",
                    dataLen, it->second->mDataLen);
            it->second->mInconsistent = true;
        }
    }

    res = GlobalDhtInstance().SaveKeyV(mNameSpace.c_str(), objId,
            mMetaDhtKey.c_str(), (char *) data, dataLen, retry);
    if (res)
    {
        LOG_ERROR(
                "Failed to set metadat: namespace:%s, objId:%s, dhtKey:%s, res:%d:%s",
                mNameSpace.c_str(), objId, mMetaDhtKey.c_str(), res, strerror(res));
        return res;
    }

    return STATUS_SUCCESS;
}

int CMetaCache::DelKeyV(const char *objId, int retry)
{
    //find the target
    int res = GlobalDhtInstance().DelKeyV(mNameSpace.c_str(), objId,
            mMetaDhtKey.c_str(), retry);
    if (res)
    {
        LOG_ERROR(
                "Failed to delete metadat: namespace:%s, objId:%s, dhtKey:%s, res:%d:%s",
                mNameSpace.c_str(), objId, mMetaDhtKey.c_str(), res, strerror(res));
    }

    std::string metaKey(objId);

    MUTEX_LOCK(&mLock);

    base_iter it = mMap.find(metaKey);

    MUTEX_UNLOCK(&mLock);

    if (it == mMap.end())
    {
        LOG_ERROR("Failed to find %s in Cache", objId);
        return res;
    }

    it->second->mDeleted = true;

    MUTEX_LOCK(&mLock);

    if (it->second->CanFree())
    {
        Erase(metaKey);
    }

    MUTEX_UNLOCK(&mLock);

    return res;
}

int CMetaCache::GetKeyV(const char *objId, char **value, int *pbuflen,
        MetaOp metaOp, int retry)
{
    std::string metaKey(objId);

    MUTEX_LOCK(&mLock);

    base_iter it = mMap.find(metaKey);

    MUTEX_UNLOCK(&mLock);

    if (it == mMap.end())
    {
        char *dhtValue = NULL;
        int dhtValueLen = 0;
        int res = GlobalDhtInstance().GetKeyV(mNameSpace.c_str(), objId,
                mMetaDhtKey.c_str(), &dhtValue, &dhtValueLen, retry);
        if (res)
        {
            if (res == ENOENT)
            {
                LOG_DEBUG(
                        "NO metadata: namespace:%s, objId:%s, dhtKey:%s, res:%d:%s",
                        mNameSpace.c_str(), objId, mMetaDhtKey.c_str(), res, strerror(res));
            } else
            {
                LOG_ERROR(
                        "Failed to get metadat: namespace:%s, objId:%s, dhtKey:%s, res:%d:%s",
                        mNameSpace.c_str(), objId, mMetaDhtKey.c_str(), res, strerror(res));
            }
            return res;
        }

        CMetaCacheValue *metaValue = new CMetaCacheValue(dhtValue, dhtValueLen);
        if (metaValue == NULL)
        {
            free(dhtValue);
            LOG_ERROR("No memory for CMetaCacheValue");
            return ENOMEM;
        }

        MUTEX_LOCK(&mLock);
        mMap.insert(pair_type(metaKey, metaValue));
        MUTEX_UNLOCK(&mLock);

        metaValue->Lock(metaOp);

        *value = dhtValue;
        *pbuflen = dhtValueLen;
    } else
    {

        //find the target
        it->second->Lock(metaOp);

        *value = (char *) it->second->mData.get();
        *pbuflen = it->second->mDataLen;
    }

    UpdateLRU(metaKey);

    return STATUS_SUCCESS;
}

int CMetaCache::PutKeyV(const char *objId, bool forceFree, MetaOp metaOp)
{
    std::string metaKey(objId);

    MUTEX_LOCK(&mLock);

    base_iter it = mMap.find(metaKey);

    MUTEX_UNLOCK(&mLock);
    if (it == mMap.end())
    {
        LOG_ERROR("Failed to find %s in Cache", objId);
        return NO_METADATA_OBJ;
    }

    if (forceFree)
    {
        it->second->mForceFree = true;
    }

    //find the target
    it->second->Unlock(metaOp);

    MUTEX_LOCK(&mLock);

    if (it->second->CanFree())
    {
        Erase(metaKey);
    }

    MUTEX_UNLOCK(&mLock);

    FreeMeta();
    return STATUS_SUCCESS;

}

int CMetaCache::AllocKeyV(const char *objId, char **value, int dataLen)
{
    std::string metaKey(objId);

    MUTEX_LOCK(&mLock);

    base_iter it = mMap.find(metaKey);

    MUTEX_UNLOCK(&mLock);

    if (it != mMap.end())
    {
        LOG_ERROR("Alloc one meta of %s, but the entry exist in cache", objId);
        return EEXIST;
    }

    char *dhtValue = (char *) malloc(dataLen);
    if (dhtValue == NULL)
    {
        LOG_ERROR("Failed to malloc memory for data in CMetaCacheValue %s",
                metaKey.c_str());
        return ENOMEM;
    }
    memset(dhtValue, 0, dataLen);

    CMetaCacheValue *metaValue = new CMetaCacheValue(dhtValue, dataLen);
    if (metaValue == NULL)
    {
        LOG_ERROR("No memory for CMetaCacheValue %s", metaKey.c_str());
        free(dhtValue);
        return ENOMEM;
    }

    metaValue->Lock(META_WRITE);

    *value = dhtValue;

    MUTEX_LOCK(&mLock);

    mMap.insert(pair_type(metaKey, metaValue));

    MUTEX_UNLOCK(&mLock);

    UpdateLRU(metaKey);

    return STATUS_SUCCESS;
}

void CMetaCache::UpdateLRU(const std::string &metaKey)
{
    MUTEX_LOCK(&mLock);
    timestamp& tm = mK2t[metaKey];
    if (tm)
    {
        // if the timestamp already exist, delete it from t2k
        mT2k.erase(tm);
    }
    tm = ++mTm;
    mT2k[tm] = metaKey; // update key in t2k
    mK2t[metaKey] = tm; // update timestamp in k2t
    MUTEX_UNLOCK(&mLock);

    FreeMeta();

}

void CMetaCache::FreeMeta()
{
    MUTEX_LOCK(&mLock);
    if (mTm != 0)
    {
        size_t freethreshold = mCapacity * DEFAULT_FREE_RATIO;
        while (mMap.size() >= freethreshold)
        {
            FreeMetaItem();
        }
    } else
    {
        //mTm is overflow
        //so force every node reload once
        for (base_iter it = mMap.begin(); it != mMap.end(); it++)
        {
            it->second->mForceFree = true;
        }LOG_INFO("CacheData timestamp overflow, begin to reload all");
    }

    MUTEX_UNLOCK(&mLock);
}

void CMetaCache::FreeMetaItem()
{
    for (T2K_iter iter = mT2k.begin(); iter != mT2k.end(); iter++)
    {
        std::string &metaKey = iter->second;

        base_iter baseIt = mMap.find(metaKey);

        if (baseIt != mMap.end())
        {
            if (baseIt->second->Empty())
            {
                Erase(metaKey);
                return;
            }
        }

    }

    return;
}

void CMetaCache::Erase(const std::string& k)
{
    // erase timestamp <-> key reference
    mT2k.erase(mK2t[k]);
    mK2t.erase(k);
    // then the actual data
    CMetaCacheValue *pMetaValue = mMap[k];
    if (pMetaValue)
    {
        delete pMetaValue;
    }
    mMap.erase(k);

}

const std::string& CMetaCache::GetDHTNameSpace()
{
    return mNameSpace;
}

const std::string& CMetaCache::GetDHTKey()
{
    return mMetaDhtKey;
}

CMetaCache*& GlobalMetaCache()
{
    static CMetaCache *gMetaCache = NULL;

    if (gMetaCache == NULL)
    {
        const std::string metaDhtKey("METADATA_KEY");
        std::string metaDhtKeyV = theGlobalProperties()->GetParamValue(
                metaDhtKey, DEFAULT_SESSION);
        if (metaDhtKeyV.empty())
        {
            LOG_ERROR("Failed to get %s", metaDhtKey.c_str());
            return gMetaCache; //NULL
        } else if (metaDhtKeyV.size() >= AFUSE_MAX_SUB_KEY_LEN)
        {
            LOG_ERROR("Key size :%s is bigger than %d",
                    metaDhtKeyV.c_str(), AFUSE_MAX_SUB_KEY_LEN);
            return gMetaCache; //NULL
        }

        u32_t capacity = DEFAULT_CAPACITY;

        const std::string cacheItemNumKey("MEATA_CACHE_ITEM_NUM");
        std::string cacheItemNumV = theGlobalProperties()->GetParamValue(
                cacheItemNumKey, DEFAULT_SESSION);
        if (cacheItemNumV.size())
        {
            Xlate::strToVal(cacheItemNumV, capacity);
        }

        gMetaCache = new CMetaCache(metaDhtKeyV, capacity);
        if (gMetaCache == NULL)
        {
            LOG_ERROR("Failed to new CMetaCache for %s", metaDhtKeyV.c_str());
            return gMetaCache; //NULL
        }
    }

    return gMetaCache;
}

CMetaCache*& GlobalDGCache()
{
    static CMetaCache *gDataGeograhyCache = NULL;

    if (gDataGeograhyCache == NULL)
    {
        const std::string metaDhtKey("FILEID_KEY");
        std::string metaDhtKeyV = theGlobalProperties()->GetParamValue(
                metaDhtKey, DEFAULT_SESSION);
        if (metaDhtKeyV.empty())
        {
            LOG_ERROR("Failed to get %s", metaDhtKey.c_str());
            return gDataGeograhyCache; //NULL
        } else if (metaDhtKeyV.size() >= AFUSE_MAX_SUB_KEY_LEN)
        {
            LOG_ERROR("Key size :%s is bigger than %d",
                    metaDhtKeyV.c_str(), AFUSE_MAX_SUB_KEY_LEN);
            return gDataGeograhyCache; //NULL
        }

        u32_t capacity = DEFAULT_CAPACITY;

        const std::string cacheItemNumKey("FILEID_CACHE_ITEM_NUM");
        std::string cacheItemNumV = theGlobalProperties()->GetParamValue(
                cacheItemNumKey, DEFAULT_SESSION);
        if (cacheItemNumV.size())
        {
            Xlate::strToVal(cacheItemNumV, capacity);
        }

        gDataGeograhyCache = new CMetaCache(metaDhtKeyV, capacity);
        if (gDataGeograhyCache == NULL)
        {
            LOG_ERROR("Failed to new CMetaCache for %s", metaDhtKeyV.c_str());
            return gDataGeograhyCache; //NULL
        }
    }

    return gDataGeograhyCache;
}
