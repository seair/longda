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
 * author: hustjackie@Longda.com
 * date:   2011/05/11
 * func:   encapsulate fdht api
 */ 

 
#include <errno.h>
#include <string.h>

#include "logger.h" //fdht logger 
#include "log.h"    //my own logger
#include "util.h"
#include "fdht.h"



#define CFDHT_RETRY_TIMES  3

CFdht::CFdht():
    mInitDht(false)
{
    MUTEX_INIT(&mLock, NULL);
}
//********************************************************************
CFdht::~CFdht()
{
    Destory();
}

//********************************************************************
void CFdht::Destory()
{
    try
    {
        if(mInitDht)
        {
            if(mKeepAlive)
            {
                
                MUTEX_LOCK(&mLock);
                for (std::map<u32_t, GroupArray *>::iterator it = mFdhtMap.begin();
                     it != mFdhtMap.end(); it++)
                {
                    GroupArray *pGroupArray = it->second;
                    fdht_disconnect_all_servers(pGroupArray);
                    delete pGroupArray;
                }
                mFdhtMap.clear();
                MUTEX_UNLOCK(&mLock);
            }
            fdht_client_destroy();
        }
        MUTEX_DESTROY(&mLock);
        LOG_INFO("Successfully destroy dht client");
    }
    catch(...)
    {
    }
}

int CFdht::InitLog()
{
    int ret = STATUS_SUCCESS;
    
    //fdfs log init
    log_init();

    if (GlobalConfigPath()->mDemon)
    {
        std::string key;
        std::string value;
        
        key = ("LOG_FILE_NAME");
        value = theGlobalProperties()->GetParamValue( key, DEFAULT_SESSION);
        if (value.size())
        {
            mLogFile = value;
        }
        
        ret = log_set_filename(mLogFile.c_str());
        if (ret)
        {
            LOG_ERROR("Failed to set fdfs owner log, rc:%d:%s", ret, strerror(ret));
            return ret;
        }
    }

    return STATUS_SUCCESS;
}

//********************************************************************
int CFdht::InitDHTClient(const std::string &szCfgFile)
{
    int rc = STATUS_INIT_FDHT;
    try
    {
        rc = InitLog();
        if (rc)
        {
            LOG_ERROR("Failed to init log");
            return rc;
            
        }
        
        rc = fdht_client_init(szCfgFile.c_str());
        if (rc)
        {
            LOG_ERROR("Failed to int fdht client, cfg:%s, rc:%d, errmsg:%s", 
                szCfgFile.c_str(), rc, strerror(rc));
            return rc;
        }
        mDhtCfg = szCfgFile;

        std::string key;
        std::string value;

        key = ("MAX_NET_CONNECTION_NUM");
        value = theGlobalProperties()->GetParamValue( key, DEFAULT_SESSION);
        if (value.size())
        {
            Xlate::strToVal(value, mMaxConnNum);
        }
        else
        {
            mMaxConnNum = MAX_CONNECTION_NUM;
        }

        for (u32_t i = 0; i < mMaxConnNum; i++)
        {

            GroupArray  * pGroupArray = new GroupArray();
            if (pGroupArray == NULL)
            {
                LOG_ERROR("Failed to alloc memory for default GroupArray");
                return ENOMEM;
            }

            mKeepAlive = true;
            if (mKeepAlive)
            {
                
                int    conn_success_count = 0;
                int    conn_fail_count    = 0;
                rc = fdht_copy_group_array(pGroupArray, &g_group_array);
                if (rc)
                {
                    LOG_ERROR("Failed to copy servers list, rc:%d:%s", rc, strerror(rc));
                    return STATUS_INIT_FDHT;
                }

                rc = fdht_connect_all_servers(pGroupArray, true, 
                    &conn_success_count, &conn_fail_count);
                if (rc)
                {
                    LOG_ERROR("fdht_connect_all_servers fail, error code:%d:%s", 
                        rc, strerror(rc));
                    return STATUS_INIT_FDHT;
                }

                //u32_t  tid = (u32_t)gettid();
                mFdhtMap[i] = pGroupArray;
            }
        }

        mDefaultGroupArray = mFdhtMap[0];
        
        mInitDht = true;
        LOG_INFO("Successfully init dht client");
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_INIT_FDHT;
}

GroupArray* CFdht::GetConnection()
{
    int rc = 0;
    
    u32_t  tid = (u32_t)gettid();

    GroupArray   *retInstance = NULL;


#if 1
    static u32_t netThreadIndex = 0;

    MUTEX_LOCK(&mLock);
    
    retInstance = mFdhtMap[netThreadIndex++];
    if (netThreadIndex >= mMaxConnNum)
    {
        netThreadIndex = 0;
    }
    
    MUTEX_UNLOCK(&mLock);

#else
    MUTEX_LOCK(&mLock);
    retInstance = mFdhtMap[tid];
    MUTEX_UNLOCK(&mLock);
#endif

    if (retInstance)
    {
        return retInstance;
    }

    GroupArray   *pGroupArray = new GroupArray();
    if (pGroupArray == NULL)
    {
        LOG_ERROR("Failed to alloc memory for %u GroupArray", tid);
        return mDefaultGroupArray;
    }
    
    int    conn_success_count = 0;
    int    conn_fail_count    = 0;
    rc = fdht_copy_group_array(pGroupArray, &g_group_array);
    if (rc)
    {
        LOG_ERROR("Failed to copy servers list for thread %u, rc:%d:%s", 
            tid, rc, strerror(rc));
        return mDefaultGroupArray;
    }

    rc = fdht_connect_all_servers(pGroupArray, true, 
        &conn_success_count, &conn_fail_count);
    if (rc)
    {
        LOG_ERROR("fdht_connect_all_servers fail for thread %u, error code:%d:%s", 
            tid, rc, strerror(rc));
        return mDefaultGroupArray;
    }

    MUTEX_LOCK(&mLock);
    mFdhtMap[tid] = pGroupArray;
    MUTEX_UNLOCK(&mLock);

    return pGroupArray;
    
}

//********************************************************************
int CFdht::CheckLength(const std::string &szNamespace, const std::string &szObjectID, const std::string &szKey)
{
    int rc = STATUS_FDHT_CMD;
    try
    {
        if (szNamespace.length() >= FDHT_MAX_NAMESPACE_LEN)
        {
            LOG_ERROR("Namespace is larger than max length, %s", szNamespace.c_str());
            return STATUS_FDHT_CMD;
        }
        if (szObjectID.length() >= FDHT_MAX_OBJECT_ID_LEN)
        {
            LOG_ERROR("objectID is larger than max length, %s", szObjectID.c_str());
            return STATUS_FDHT_CMD;
        }

        if (szKey.length() >= FDHT_MAX_SUB_KEY_LEN)
        {
            LOG_ERROR("key is larger than max length, %s", szKey.c_str());
            return STATUS_FDHT_CMD;
        }
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_FDHT_CMD;
}

int CFdht::SaveKeyV(const char *nameSpace, const char *objId, const char *key,
                    const char *value, unsigned int valueLen, int retry)
{
    int rc = STATUS_FDHT_CMD;
    try
    {
        GroupArray *pGroupArray = GetConnection();
        
        FDHTKeyInfo keyInfo;
        memset(&keyInfo, 0, sizeof(FDHTKeyInfo));
        if(nameSpace && strlen(nameSpace) != 0)
        {
            strncpy(keyInfo.szNameSpace, nameSpace, FDHT_MAX_NAMESPACE_LEN -1);
            keyInfo.namespace_len = strlen(keyInfo.szNameSpace);
        }

        if (objId && strlen(objId) != 0)
        {
            strncpy(keyInfo.szObjectId, objId, FDHT_MAX_OBJECT_ID_LEN - 1);
            keyInfo.obj_id_len = strlen(keyInfo.szObjectId);
        }

        if (key && strlen(key) != 0)
        {
            strncpy(keyInfo.szKey, key, FDHT_MAX_SUB_KEY_LEN - 1);
            keyInfo.key_len = strlen(keyInfo.szKey);
        }

        for (int i = 0; i < retry; i++)
        {

            rc = fdht_set_ex(pGroupArray, mKeepAlive, &keyInfo, 
                    FDHT_EXPIRES_NEVER, value, valueLen);
            if(rc)
            {
                LOG_ERROR("Failed to set dht, namespace:%s, objectID:%s, key:%s, rc:%d:%s",
                    keyInfo.szNameSpace, keyInfo.szObjectId, keyInfo.szKey, rc, strerror(rc));
            }
            else
            {
                break;
            }
            //sleep(1);
        }
            
        return rc;
    }
    catch(...)
    {
    }
    return STATUS_FDHT_CMD;
}
//********************************************************************
int CFdht::GetKeyV(const char *nameSpace, const char *objId, const char *key,
                   char **value, int *pbuflen, int retry)
{
    int rc = STATUS_FDHT_CMD;
    try
    {
        GroupArray *pGroupArray = GetConnection();

        FDHTKeyInfo keyInfo;
        memset(&keyInfo, 0, sizeof(FDHTKeyInfo));
        if(nameSpace && strlen(nameSpace) != 0)
        {
            strncpy(keyInfo.szNameSpace, nameSpace, FDHT_MAX_NAMESPACE_LEN -1);
            keyInfo.namespace_len = strlen(keyInfo.szNameSpace);
        }

        if (objId && strlen(objId) != 0)
        {
            strncpy(keyInfo.szObjectId, objId, FDHT_MAX_OBJECT_ID_LEN - 1);
            keyInfo.obj_id_len = strlen(keyInfo.szObjectId);
        }

        if (key && strlen(key) != 0)
        {
            strncpy(keyInfo.szKey, key, FDHT_MAX_SUB_KEY_LEN - 1);
            keyInfo.key_len = strlen(keyInfo.szKey);
        }

        for (int i = 0; i < retry; i++)
        {

            rc = fdht_get_ex1(pGroupArray, mKeepAlive, &keyInfo, 
                FDHT_EXPIRES_NONE, value, pbuflen, malloc);
            if(rc)
            {
                if (rc == ENOENT)
                {
                    LOG_DEBUG("No entry, namespace:%s, objectID:%s, key:%s, rc:%d:%s",
                        keyInfo.szNameSpace, keyInfo.szObjectId, keyInfo.szKey, rc, strerror(rc));
                }
                else
                {
                    LOG_ERROR("Failed to get dht, namespace:%s, objectID:%s, key:%s, rc:%d:%s",
                        keyInfo.szNameSpace, keyInfo.szObjectId, keyInfo.szKey, rc, strerror(rc));

                }
            }
            else
            {
                break;
            }
        }
            
        return rc;
    }
    catch(...)
    {
    }
    return STATUS_FDHT_CMD;
}

int CFdht::DelKeyV(const char *nameSpace, const char *objId, const char *key, int retry)
{
    int rc = STATUS_FDHT_CMD;
    try
    {
        GroupArray *pGroupArray = GetConnection();

        
        FDHTKeyInfo keyInfo;
        memset(&keyInfo, 0, sizeof(FDHTKeyInfo));
        if(nameSpace && strlen(nameSpace) != 0)
        {
            strncpy(keyInfo.szNameSpace, nameSpace, FDHT_MAX_NAMESPACE_LEN -1);
            keyInfo.namespace_len = strlen(keyInfo.szNameSpace);
        }

        if (objId && strlen(objId) != 0)
        {
            strncpy(keyInfo.szObjectId, objId, FDHT_MAX_OBJECT_ID_LEN - 1);
            keyInfo.obj_id_len = strlen(keyInfo.szObjectId);
        }

        if (key && strlen(key) != 0)
        {
            strncpy(keyInfo.szKey, key, FDHT_MAX_SUB_KEY_LEN - 1);
            keyInfo.key_len = strlen(keyInfo.szKey);
        }

        for (int i = 0; i < retry; i++)
        {

            rc = fdht_delete_ex(pGroupArray, mKeepAlive, &keyInfo);
            if(rc)
            {
                LOG_ERROR("Failed to delete dht, namespace:%s, objectID:%s, key:%s, rc:%d:%s",
                    keyInfo.szNameSpace, keyInfo.szObjectId, keyInfo.szKey, rc, strerror(rc));
                if (rc != ENOENT)
                {
                    //sleep(1);
                }
            }
            else
            {
                break;
            }
        }
            
        return rc;
    }
    catch(...)
    {
    }
    return STATUS_FDHT_CMD;
}

int CFdht::Test()
{
    int ret = STATUS_SUCCESS;

    time_t now;
    time(&now);
    char key[FDHT_MAX_SUB_KEY_LEN] = {0};
    snprintf(key, sizeof(key), "%lld", (long long)now);
    char value[FDHT_MAX_SUB_KEY_LEN] = {0};
    strcpy(value, key);

    ret = SaveKeyV(NULL, NULL, key, value, strlen(value));
    if (ret)
    {
        LOG_ERROR("Failed to save key:%s, value:%s, rc:%d:%s", 
            key, value, ret, strerror(ret));
        return ret;
    }

    char *value2 = NULL;
    int   valueLen = 0;
    ret = GetKeyV(NULL, NULL, key, &value2, &valueLen);
    if (ret)
    {
        LOG_ERROR("Failed to get key:%s, rc:%d:%s", 
            key, ret, strerror(ret));
        return ret;
    }

    if (strcmp(value, value2) != 0 ||
        strlen(value) != valueLen )
    {
        LOG_ERROR("Data are not equal, before:%s:%d, after:%s:%d",
            value, strlen(value), value2, valueLen);
        return STATUS_UNKNOW_ERROR;
    }

    ret = DelKeyV(NULL, NULL, key);
    if (ret)
    {
        LOG_ERROR("Failed to delete key:%s", key);
        return ret;
    }

    ret = GetKeyV(NULL, NULL, key, &value2, &valueLen);
    if (ret != ENOENT)
    {
        LOG_ERROR("After delete, still get :%s", key);
        return STATUS_UNKNOW_ERROR;
    }

    return STATUS_SUCCESS;
    
}

CFdht &GlobalDhtInstance()
{
    static CFdht gDht;
    return gDht;
}

int InitDht()
{
    const std::string dhtCfgKey("DHT_CFG");
    std::string       dhtCfg;
    dhtCfg = theGlobalProperties()->GetParamValue(dhtCfgKey, DEFAULT_SESSION);
    if (dhtCfg.empty())
    {
        LOG_ERROR("Failed to get DHT configuration file");
        return STATUS_INIT_FDHT;
    }

    int rc = STATUS_SUCCESS;
    rc = GlobalDhtInstance().InitDHTClient(dhtCfg);
    if (rc)
    {
        LOG_ERROR("Failed to int DHT client");
        return rc;
    }

    LOG_INFO("Successfully init DHT");

    return STATUS_SUCCESS;
}


