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

#ifndef __CFDHT_H__
#define __CFDHT_H__
//********************************************************************
#include <string>
#include <iostream>
#include <map>

#include "fdht_global.h"
#include "fdht_types.h"
#include "fdht_proto.h"
#include "fdht_client.h"
#include "fdht_func.h"

#include "defs.h"
#include "mutex.h"


#define AFUSE_MAX_NAMESPACE_LEN  FDHT_MAX_NAMESPACE_LEN
#define AFUSE_MAX_SUB_KEY_LEN    FDHT_MAX_SUB_KEY_LEN

//********************************************************************
class CFdht
{
public:
    CFdht();
    ~CFdht();
    
    /* Init DHT client
     * @param[in]: szCfgFile     dht client configure file
     * @return                   0 -- success, others failed
     */
    int InitDHTClient(const std::string &szCfgFile);

    /* Check namespace, objectid, key length is invalid or not
     * @param[in] szNamespace     namespace
     * @param[in] szObjectID      objectid
     * @param[in] szKey           key
     * @return                    0 -- success, others failed
     */
    int CheckLength(const std::string &szNamespace, const std::string &szObjectID, const std::string &szKey);

    /* Save the key-value pair into DHT
     * @param[in] szNamespace     namespace
     * @param[in] szObjectID      objectid
     * @param[in] szKey           key
     * @param[in] value           data 
     * @param[in] valueLen        data's length
     * @param[in] retry           retry times
     * @return                    0 -- success, others failed
     */
    int SaveKeyV(const char *nameSpace, const char *objId, const char *key,
                    const char *value, unsigned int valueLen, int retry = 3);

    /* Get the key-value pair from DHT
     * @param[in] szNamespace     namespace
     * @param[in] szObjectID      objectid
     * @param[in] szKey           key
     * @param[out]value           data 
     * @param[out]pbuflen         data's length
     * @param[in] retry           retry times
     * @return                    0 -- success, others failed
     */
    int GetKeyV(const char *nameSpace, const char *objId, const char *key,
                   char **value, int *pbuflen, int retry = 3);

    /* Delete the key-value pair from DHT
     * @param[in] szNamespace     namespace
     * @param[in] szObjectID      objectid
     * @param[in] szKey           key
     * @param[in] retry           retry times
     * @return                    0 -- success, others failed
     */
    int DelKeyV(const char *nameSpace, const char *objId, const char *key, int retry = 3);

    /*
     * test DHT service is fine or not
     * @return                    0 -- success, others failed
     */
    int Test();

protected:
    void        Destory();
    GroupArray* GetConnection();

    int         InitLog();
    
private:
    std::string mDhtCfg;
    std::string mLogFile;
    bool        mInitDht;

    
    
    GroupArray  *mDefaultGroupArray;
    bool         mKeepAlive;

    std::map<u32_t, GroupArray *>   mFdhtMap;
    pthread_mutex_t                 mLock;
    u32_t                           mMaxConnNum;
};

//********************************************************************

CFdht &GlobalDhtInstance();
int    InitDht();

#endif //__CFDHT_H__
