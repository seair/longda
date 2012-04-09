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
 * func:   Encapsulate fdfs client API
 */ 

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "fastcommon/sockopt.h"
#include "fastcommon/logger.h" //fdfs logger 
#include "fdfs.h"

#include "log.h"    //my own logger
#include "util.h"




#define CFDFS_RETRY_TIMES      3

//copy from fdfs storage_client.c
#define FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)               \
    char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];              \
    char *group_name;                                             \
    char *filename;                                               \
    char *pSeperator;                                             \
                                                                  \
    snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);    \
    pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);     \
    if (pSeperator == NULL)                                       \
    {                                                             \
        return EINVAL;                                            \
    }                                                             \
                                                                  \
    *pSeperator = '\0';                                           \
    group_name = new_file_id;                                     \
    filename =  pSeperator + 1;                                   \


class DownloadBufferParm
{
public:
    DownloadBufferParm(char *buffer, const u32_t bufLen):
        mBuffer(buffer),
        mCurBuffer(buffer),
        mBufferEnd(buffer + bufLen)
    {
    }
    int Copy(const s64_t fileSize, const char *data, const int curSize)
    {
        if (mCurBuffer + curSize > mBufferEnd)
        {
            return ENOMEM;
        }
        memcpy(mCurBuffer, data, curSize);
        mCurBuffer += curSize;
        return 0;
    }
private:
    char     *mBuffer;
    char     *mCurBuffer;
    char     *mBufferEnd;
};

int DownloadToBuffCb(void *arg, const int64_t fileSize, 
    const char *data, const int curSize)
{
    DownloadBufferParm *dbParm = (DownloadBufferParm *)arg;

    return dbParm->Copy(fileSize, data, curSize);
}

int WriteToFileCb(void *arg, const int64_t fileSize, const char *data, const int currentSize)
{
    if (arg == NULL)
    {
        return EINVAL;
    }

    int fd = *(int *)arg;
    if (write(fd, data, currentSize) < currentSize)
    {
        return errno != 0 ? errno : EIO;
    }

  return 0;
}



ServerInfo::ServerInfo(const FDFSStorageInfo    &storageStat, const char *groupName, const u32_t maxConnNum)
{
    mMaxConnNum = maxConnNum;
    
    mConnections = new TrackerServerInfo[mMaxConnNum];
    
    ASSERT(mConnections, "No memory for TrackerServerInfo of %s:%s", groupName, storageStat.ip_addr);

    memset(mConnections, 0, mMaxConnNum * sizeof(TrackerServerInfo));

    for (u32_t i = 0; i < mMaxConnNum; i++)
    {
        TrackerServerInfo *pConnction = &mConnections[i];
        
        pConnction->sock = -1;
        pConnction->port = storageStat.storage_port;
        strcpy(pConnction->ip_addr, storageStat.ip_addr);
        strcpy(pConnction->group_name, groupName); 
    }

    MUTEX_INIT(&mLock, NULL);

    ResetStat(storageStat);
    
}

ServerInfo::~ServerInfo()
{
    for (u32_t i = 0; i < mMaxConnNum; i++)
    {
        TrackerServerInfo *pConnction = &mConnections[i];
        tracker_disconnect_server(pConnction);
    }

    delete mConnections;

    MUTEX_DESTROY(&mLock);
    
}

u32_t ServerInfo::GetNextDisk()
{
    u32_t   ret = (u32_t)Random((long int)mTotalDiskNum);

    return ret;
}

TrackerServerInfo* ServerInfo::GetServerConn()
{
    static u32_t    serverIndex = 0;

    TrackerServerInfo* ret = NULL;

    MUTEX_LOCK(&mLock);

    ret = &mConnections[serverIndex++];

    if (serverIndex >= mMaxConnNum)
    {
        serverIndex = 0;
    }
    
    MUTEX_UNLOCK(&mLock);

    return ret;
}

void ServerInfo::ResetStat(const FDFSStorageInfo    &storageStat)
{
    mStat = storageStat;
    mTotalDiskNum = (storageStat.store_path_count);
}

GroupInfo::GroupInfo():
    mOpCount(0),
    mFreeCapacity(0),
    mCurServerIndex(0)
{
    memset(&mGroupStat, 0, sizeof(mGroupStat));
}

GroupInfo::~GroupInfo()
{
    for (std::map<std::string, ServerInfo *>::iterator it = mServerMap.begin();
             it != mServerMap.end(); it++)
     {
        ServerInfo *pServer = it->second;
        delete pServer;
     }
     mServerMap.clear();
}

void GroupInfo::UpdateStat()
{
    u64_t        opCount = 0;
    u64_t        freeCapacity = 0;
    for (std::map<std::string, ServerInfo *>::iterator it = mServerMap.begin();
             it != mServerMap.end(); it++)
     {
        ServerInfo *pServer = it->second;

        opCount += pServer->mStat.stat.total_upload_count;
        opCount += pServer->mStat.stat.total_append_count;
        opCount += pServer->mStat.stat.total_set_meta_count;
        opCount += pServer->mStat.stat.total_delete_count;
        opCount += pServer->mStat.stat.total_download_count;
        opCount += pServer->mStat.stat.total_get_meta_count;

        if (freeCapacity == 0)
        {
            freeCapacity = pServer->mStat.free_mb;
        }
        else if (freeCapacity > pServer->mStat.free_mb)
        {
            freeCapacity = pServer->mStat.free_mb;
        }
     }

     mOpCount = opCount;
     mFreeCapacity = freeCapacity * 1024 * 1024;
}

int GroupInfo::GetStorageServer(TrackerServerInfo ** ppStorageServer, int * pStorePathIndex)
{
    int randomIndex = (int)Random((long int)mServerMap.size());
    ServerInfo *pServerInfo = NULL;

    int i = 0;
    for (std::map<std::string, ServerInfo *>::iterator it = mServerMap.begin() ;
         it != mServerMap.end(); it++, i++)
    {
        if (i == randomIndex)
        {
            pServerInfo = it->second;
            *ppStorageServer = pServerInfo->GetServerConn();
            *pStorePathIndex = pServerInfo->GetNextDisk();
            return STATUS_SUCCESS;
        }
    }
    LOG_ERROR("Failed to get StorageServer ERROR %d:%s", ENOENT, strerror(ENOENT));
    return ENOENT;
}

//********************************************************************
CFdfs::CFdfs():
    mCurGroupIndex(0),
    mInit(false),
    mUploadType(ROLL),
    mShortConn(false),
    mTrackerServers(NULL),
    mTrackerGroups(NULL),
    mMaxConnNum(MAX_CONNECTION_NUM)
{
    MUTEX_INIT(&mLock, NULL);
}
    
//********************************************************************
CFdfs::~CFdfs()
{
    Destory();
}

//********************************************************************
void CFdfs::Destory()
{
    try
    {
        for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++)
        {
            GroupInfo *pGroupInfo = it->second;
            delete pGroupInfo;
         }
         mGroupMap.clear();
        
        if(mInit)
        {
            for (u32_t i = 0; i < mMaxConnNum; i++)
            {
                if (mTrackerServers[i])
                {
                    fdfs_quit(mTrackerServers[i]);
                    tracker_disconnect_server(mTrackerServers[i]);  
                }

                fdfs_client_destroy_ex(&mTrackerGroups[i]);
            }

            delete mTrackerServers;
            mTrackerServers = NULL;

            delete mTrackerGroups;
            mTrackerGroups = NULL;

            fdfs_client_destroy();

            log_destroy();
        }

        mInit = false;
        
        MUTEX_DESTROY(&mLock);

        LOG_INFO("Destroy one dfs client");
    }
    catch(...)
    {
    }
}

int CFdfs::SetProperies()
{
    std::string key;
    std::string value;

    key = ("DFS_CFG");
    value = theGlobalProperties()->GetParamValue( key, DEFAULT_SESSION);
    if (value.empty())
    {
        LOG_ERROR("Failed to get DFS configuration file");
        return STATUS_INIT_FDFS;
    }
    mDFSCfg = value;

    key = "UPLOAD_BALANCE";
    value = theGlobalProperties()->GetParamValue(key, DEFAULT_SESSION);
    if (value.size())
    {
        u32_t uploadType = 0;
        Xlate::strToVal(value, uploadType);

        if (CFdfs::ROLL <= uploadType && uploadType < CFdfs::UPLOAD_END)
        {
            mUploadType = (UPLOAD_TYPE)uploadType;
        }
    }

    key = "FDFS_SHORT_CONN";
    value = theGlobalProperties()->GetParamValue(key, DEFAULT_SESSION);
    if (value.size())
    {
        Xlate::strToVal(value, mShortConn);
    }

    key = ("MAX_NET_CONNECTION_NUM");
    value = theGlobalProperties()->GetParamValue( key, DEFAULT_SESSION);
    if (value.size())
    {
        Xlate::strToVal(value, mMaxConnNum);
    }

    key = ("LOG_FILE_NAME");
    value = theGlobalProperties()->GetParamValue( key, DEFAULT_SESSION);
    if (value.size())
    {
        mLogFile = value;
    }
    
    return STATUS_SUCCESS;
}

int CFdfs::InitLog()
{
    int ret = STATUS_SUCCESS;
    
    //fdfs log init
    log_init();

    if (GlobalConfigPath()->mDemon)
    {
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
int CFdfs::InitDFSConnection()
{
    int rc = STATUS_INIT_FDFS;
    try
    {
        rc = SetProperies();
        if (rc)
        {
            LOG_ERROR("Failed to set property");
            return rc;
        }

        rc = InitLog();
        if (rc)
        {
            LOG_ERROR("Failed to init fdfs log");
            return rc;
        }
        
        // init global fdfs client
        rc = fdfs_client_init(mDFSCfg.c_str());
        if (rc)
        {
            LOG_ERROR("Failed to do fdfs_client_init, rc:%d:%s", rc, strerror(rc));
            return rc;
        }

        mTrackerGroups = new TrackerServerGroup[mMaxConnNum];
        if (mTrackerGroups == NULL)
        {
            LOG_ERROR("Failed to alloc memory for TrackerServerGroup");
            return ENOMEM;
        }
        memset(mTrackerGroups, 0, mMaxConnNum * sizeof(TrackerServerGroup));

        mTrackerServers = new TrackerServerInfo*[mMaxConnNum];
        if (mTrackerServers == NULL)
        {
            LOG_ERROR("Failed to alloc memory for TrackerServerInfo");
            return ENOMEM;
        }
        memset(mTrackerServers, 0, mMaxConnNum * sizeof(TrackerServerInfo*));

        for (u32_t i = 0; i < mMaxConnNum; i++)
        {
            rc = fdfs_load_tracker_group(&mTrackerGroups[i], mDFSCfg.c_str());
            if (rc)
            {
                LOG_ERROR("Failed to fdfs_load_tracker_group, rc %d:%s", rc, strerror(rc));
                return rc;
            }
            mTrackerServers[i] = tracker_get_connection_ex(&mTrackerGroups[i]);
            if (mTrackerServers[i] == NULL)
            {
                LOG_ERROR("Failed to do tracker_get_connection_ex, index %d, error:%d, errmsg:%s", 
                    i, errno, strerror(errno));
                return STATUS_INIT_FDFS;
            }
                
        }
        
        rc = GetAllGroups();
        if (rc)
        {
            LOG_ERROR("Failed to get groups information");
            return rc;
        }
        
        rc = GetAllServers();
        if (rc)
        {
            LOG_ERROR("Failed to connect all servers");
            return rc;
        }
        
        mInit = true;
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_INIT_FDFS;
}

TrackerServerInfo* CFdfs::GetTrackerConn(u32_t &trackerIndex)
{
    static u32_t   connectionIndex = 0;
 
    TrackerServerInfo  *ret = NULL;

    MUTEX_LOCK(&mLock);

    trackerIndex = connectionIndex++;
    
    ret = mTrackerServers[trackerIndex];

    if (connectionIndex >= mMaxConnNum)
    {
        connectionIndex = 0;
    }

    MUTEX_UNLOCK(&mLock);

    return ret;
}

void CFdfs::SetTrackerConn(TrackerServerInfo *pTrackerServer, u32_t index)
{
    if (index < mMaxConnNum)
    {
        MUTEX_LOCK(&mLock);

        mTrackerServers[index] = pTrackerServer;

        MUTEX_UNLOCK(&mLock);
    }
   
    return ;
}

int CFdfs::ConnectTracker(TrackerServerInfo  **ppTrackerServer, const u32_t trackerIndex)
{
    int rc = STATUS_INIT_FDFS;
    try
    {
        if (*ppTrackerServer)
        {
            fdfs_quit(*ppTrackerServer);
            tracker_disconnect_server(*ppTrackerServer);
            *ppTrackerServer = NULL;
        }
        
        *ppTrackerServer = tracker_get_connection_ex(&mTrackerGroups[trackerIndex]);
        if (*ppTrackerServer == NULL)
        {
            LOG_ERROR("Failed to do tracker_get_connection_ex, error:%d, errmsg:%s", 
                errno, strerror(errno));
            return STATUS_FDFS_CONNECT;
        }

        SetTrackerConn(*ppTrackerServer, trackerIndex);
        
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_INIT_FDFS;
}

int CFdfs::GetAllServers()
{
    u32_t               trackerIndex = 0;
    TrackerServerInfo  *pTrackerServer = GetTrackerConn(trackerIndex);
    
    for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++)
    {
        const std::string &groupName = it->first;
        
        FDFSStorageInfo storages[FDFS_MAX_SERVERS_EACH_GROUP];

        int storageCount = 0;

        int rc = tracker_list_servers(pTrackerServer, \
                groupName.c_str(), NULL, \
                storages, FDFS_MAX_SERVERS_EACH_GROUP, \
                &storageCount);
        if (rc != 0)
        {
            LOG_ERROR("Failed to get %s's servers, rc %d:%s", 
                groupName.c_str(), rc, strerror(rc));
            return rc;
        }
        else if (storageCount == 0)
        {
            LOG_ERROR("One group %s storage shutdown ", groupName.c_str());
            return ENOENT;
        }

        GroupInfo *pGroupInfo = it->second;
        for (int i = 0; i < storageCount; i++)
        {
            std::string serverName;
            if (GetHostname(storages[i].ip_addr, serverName))
            {
                serverName = storages[i].ip_addr;
            }

            ServerInfo *pServerInfo = pGroupInfo->mServerMap[serverName];
            if (pServerInfo == NULL)
            {
                pServerInfo = new ServerInfo(storages[i], groupName.c_str(), mMaxConnNum);
                if (pServerInfo == NULL)
                {
                    LOG_ERROR("Failed to alloc memory for %s ServerInof",
                        serverName.c_str());
                    return ENOMEM;
                }
                pGroupInfo->mServerMap[serverName] = pServerInfo;
            }
            else
            {
                pServerInfo->ResetStat(storages[i]);
            }
            
            
        }

        pGroupInfo->UpdateStat();
    }



    return STATUS_SUCCESS;
}

u64_t CFdfs::GetFreeCapacity()
{
    u64_t        freeCapcity = 0;
    for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++)
    {
        GroupInfo *pGroup = it->second;

        freeCapcity += pGroup->mFreeCapacity;
    }

    freeCapcity = freeCapcity;
    return freeCapcity;
}

int CFdfs::GetAllGroups()
{
    u32_t               trackerIndex = 0;
    TrackerServerInfo  *pTrackerServer = GetTrackerConn(trackerIndex);

    FDFSGroupStat groups[FDFS_MAX_GROUPS];

    int  groupCount = 0;

    int rc = tracker_list_groups(pTrackerServer, \
        groups, FDFS_MAX_GROUPS, \
        &groupCount);
    if (rc != 0)
    {
        LOG_ERROR("Failed to list all groups, rc %d:%s", rc, strerror(rc));
        return rc;
    }
    else if (groupCount == 0)
    {
        LOG_ERROR("The storage group number is 0");
        return ENOENT;
    }

    for (int i = 0; i < groupCount; i++)
    {
        GroupInfo *pGroup = mGroupMap[groups[i].group_name];
        if (pGroup == NULL)
        {
            pGroup = new GroupInfo();
            if (pGroup == NULL)
            {
                LOG_ERROR("Failed to alloc memory for GroupInfo of %s",
                    groups[i].group_name);
                return ENOMEM;
            }
            mGroupMap[groups[i].group_name] = pGroup;
        }

        pGroup->mGroupStat = groups[i];
        pGroup->mFreeCapacity = groups[i].free_mb * 1024 * 1024;
    }

    if (mGroupMap.size() != groupCount)
    {
        for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++)
        {
            const std::string &groupName = it->first;
            bool         found     = false;

            for (int i = 0; i < groupCount; i++)
            {
                if (strcmp(groupName.c_str(), groups[i].group_name) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (found == false)
            {
                LOG_ERROR("%s group info haven't been found from tracker's group list", 
                    groupName.c_str());
            }
        }
    }
    

    return STATUS_SUCCESS;
}

//This function will query from server
int CFdfs::GetStorageServer(TrackerServerInfo  *pTrackerServer,
                            TrackerServerInfo  *pStorageServer, 
                            int * pStorePathIndex, const std::string szGroupName)
{
    int rc = STATUS_FDFS_CONNECT;
    try
    {
        //tracker_query_storage_store_without_group will memset pStorageServer
        //获取可用StorageServer
        if(szGroupName.empty())
        {
            rc = tracker_query_storage_store_without_group(pTrackerServer, pStorageServer, pStorePathIndex);
            if(rc)
            {
                LOG_ERROR("Failed to get storage server, pTrackerServer %p:%s, error:%d, errmsg:%s", 
                    pTrackerServer, pTrackerServer->ip_addr, rc, strerror(rc));
                return rc;
            }
        }
        else
        {
            rc = tracker_query_storage_store_with_group(pTrackerServer, \
                szGroupName.c_str(), pStorageServer, pStorePathIndex);
            if(rc)
            {
                LOG_ERROR("Failed to get storage server group :%s, pTrackerServer %p:%s, error:%d, errmsg:%s", 
                    szGroupName.c_str(), pTrackerServer, pTrackerServer->ip_addr, rc, strerror(rc));
                return rc;
            }
        }
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_FDFS_CONNECT;
}

int CFdfs::GetStorageServer(TrackerServerInfo ** ppStorageServer, int * pStorePathIndex, const  std::string szGroupName)
{
    ASSERT(ppStorageServer, "The input StorageServer point is NULL");

    GroupInfo *pGroupInfo = NULL;

    if (szGroupName.size())
    {
        std::map<std::string, GroupInfo *>::iterator it = mGroupMap.find(szGroupName);
        if (it != mGroupMap.end())
        {
            pGroupInfo = it->second;
            if (pGroupInfo)
            {
                return pGroupInfo->GetStorageServer( ppStorageServer, pStorePathIndex);
            }
        }
    }

    if (mUploadType == ROLL)
    {
        u32_t cur = mCurGroupIndex++;
        if (cur >= mGroupMap.size())
        {
            LOG_WARN("Warning current index %u exceed mGroupIndex size %u",
                cur, (u32_t)mGroupMap.size());
            cur = 0;
        }
        if (mCurGroupIndex >= mGroupMap.size())
        {
            mCurGroupIndex = 0;
        }

        int index = 0;
        for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++, index++)
        {
            if (index == cur)
            {
                pGroupInfo = it->second;
                break;
            }
        }
             
        if (pGroupInfo)
        {
            return pGroupInfo->GetStorageServer( ppStorageServer, pStorePathIndex);
        }
   
    }

    if (mUploadType == OPERATION_NUM)
    {
        u64_t   leastOpNum = 0;
        for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++)
        {
            GroupInfo *pTemp = it->second;
            if (leastOpNum == 0)
            {
                pGroupInfo = pTemp;

                leastOpNum = pTemp->mOpCount;
            }
            else if (leastOpNum > pTemp->mOpCount)
            {
                pGroupInfo = pTemp;

                leastOpNum = pTemp->mOpCount;
            }
        }

        if (pGroupInfo)
        {
            return pGroupInfo->GetStorageServer( ppStorageServer, pStorePathIndex);
        }
    }

    if (mUploadType == CAPACITY)
    {
        u64_t   leastCapacity = 0;
        for (std::map<std::string, GroupInfo *>::iterator it = mGroupMap.begin();
             it != mGroupMap.end(); it++)
        {
            GroupInfo *pTemp = it->second;
            if (leastCapacity == 0)
            {
                pGroupInfo = pTemp;

                leastCapacity = pTemp->mFreeCapacity;
            }
            else if (leastCapacity > pTemp->mFreeCapacity)
            {
                pGroupInfo = pTemp;

                leastCapacity = pTemp->mFreeCapacity;
            }
        }

        if (pGroupInfo)
        {
            return pGroupInfo->GetStorageServer( ppStorageServer, pStorePathIndex);
        }
    }

    return ENOENT;
}


//********************************************************************
int CFdfs::ConnectStorageServer(TrackerServerInfo *pStorageServer)
{
    int rc = STATUS_FDFS_CONNECT;
    try
    {
        //连接StorageServer
        rc = tracker_connect_server(pStorageServer);
        if (rc)
        {
            LOG_ERROR("Failed to connect storage, error:%d, errmsg:%s", rc, strerror(rc));
            return rc;
        }   
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_FDFS_CONNECT;
}
//********************************************************************
int CFdfs::DisConnectServer(TrackerServerInfo *pServer)
{
    int rc = STATUS_FDFS_CONNECT;
    try
    {
        tracker_disconnect_server(pServer);  
        return STATUS_SUCCESS;
    }
    catch(...)
    {
    }
    return STATUS_FDFS_CONNECT;
}

//********************************************************************
int CFdfs::DownloadCommon(TrackerServerInfo   *pTrackerServer, 
                          TrackerServerInfo   *pStorageServer, 
                          const char          *group_name, 
                          const char          *filename, 
                          CDownloadParam       &downloadParam, 
                          s64_t                *pFileSize)
{
    int rc = STATUS_SUCCESS;

    if (downloadParam.mType == FDFS_DATA_FILE)
    {
        const char *pLocalFile = downloadParam.mLocalFile;
        rc = storage_download_file_to_file( 
                        pTrackerServer, pStorageServer, 
                        group_name, filename, 
                        pLocalFile, (int64_t *)pFileSize);
        if (rc)
        {
            LOG_ERROR("Failed to download %s/%s to %s, server %p, error:%d, errmsg:%s",
                        group_name, filename, pLocalFile, 
                        pStorageServer, rc, strerror(rc));
            return rc;
        }
        return STATUS_SUCCESS;
    }
    else if (downloadParam.mType == FDFS_DATA_PART_FILE)
    {
        const char *pLocalFile = downloadParam.mLocalFile;
        const s64_t offset     = downloadParam.mOffset;
        
        int fd = open(pLocalFile, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IROTH);
        if (fd < 0)
        {
            LOG_ERROR("Failed to open file %s, rc:%d:%s", 
                pLocalFile, errno, strerror(errno));
            return errno;
        }
        if (lseek(fd, offset, SEEK_SET) < 0) 
        {
            LOG_ERROR("Failed to lseek file %s, rc:%d:%s", 
                pLocalFile, errno, strerror(errno));
            close(fd);
            return errno;
        }

        rc = storage_download_file_ex(pTrackerServer, \
            pStorageServer, \
            group_name, filename, \
            0, 0, \
            WriteToFileCb, (void *)&fd, (int64_t *)pFileSize);
        if (rc)
        {
            LOG_ERROR("Failed to download %s/%s to buffer %s, server %p, error:%d, errmsg:%s",
                    group_name, filename, pLocalFile,
                    pStorageServer, rc, strerror(rc));
            close(fd);
            return rc;
        }
        close(fd);
        return STATUS_SUCCESS;
    }
    else if (downloadParam.mType == FDFS_DATA_BUFFER)
    {
        char        *pBuffer   = downloadParam.mBuffer;
        const u32_t  bufLen    = downloadParam.mBufLen;
        DownloadBufferParm dbParam(pBuffer, bufLen);
        rc = storage_download_file_ex(pTrackerServer, 
            pStorageServer, 
            group_name, filename, 
            0, 0, 
            DownloadToBuffCb, (void *)&dbParam, (int64_t *)pFileSize);
        if (rc)
        {

            LOG_ERROR("Failed to download %s/%s to buffer %p, server %p, error:%d, errmsg:%s",
                            group_name, filename, pBuffer,
                            pStorageServer, rc, strerror(rc));
            return rc;
        }
        return STATUS_SUCCESS;
    }
    else
    {
        LOG_ERROR("Unknown type");
        return EINVAL;
    }

    return     EINVAL;
}


int CFdfs::DownloadCommon(CDownloadParam &downloadParam, s64_t *pFileSize)
{
    int rc = STATUS_FDFS_CMD;
    const char *blockId = downloadParam.mBlockId;
    try
    {
        FDFS_SPLIT_GROUP_NAME_AND_FILENAME(blockId);

        bool  shortConn = mShortConn;

        u32_t                trackerIndex   = 0;
        TrackerServerInfo   *pTrackerServer = GetTrackerConn(trackerIndex);
        
        for (int i = 0; i < CFDFS_RETRY_TIMES; i++)
        {
            if (pTrackerServer == NULL)
            {
                rc = ConnectTracker(&pTrackerServer, trackerIndex);
                if (rc)
                {
                    LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                        trackerIndex, rc, strerror(rc));
                    continue;
                }
            }
            
            TrackerServerInfo *pStorageServer = NULL;
            if (shortConn == false)
            {
                rc = GetStorageByBlockid(blockId, &pStorageServer);
                if (rc)
                {
                    LOG_WARN("Failed to get StorageInfo from blockId %s", blockId);
                    shortConn = true;
                    continue;
                }

                if (pStorageServer->sock < 0)
                {
                    //the sock has been broken
                    rc = ConnectStorageServer(pStorageServer);
                    if (rc)
                    {
                        LOG_ERROR("Failed to connect storage server %p:%s, rc %d:%s",
                            pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                        shortConn = true;
                        continue;
                    }
                }
                
            }

            rc = DownloadCommon( 
                    pTrackerServer, pStorageServer, 
                    group_name, filename, 
                    downloadParam, pFileSize);
            if(rc)
            {
                if (shortConn)
                {
                    int tempRc = ConnectTracker(&pTrackerServer, trackerIndex);
                    if (tempRc)
                    {
                        LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                            trackerIndex, tempRc, strerror(tempRc));
                    }
                    
                }
                else
                {
                    DisConnectServer(pStorageServer);
                }
                shortConn = true;
                continue;
            }
            else
            {
                
                UpdatLocalStat(group_name, 0, 0);
                return STATUS_SUCCESS;
            }

            
        }
        return rc;
    }
    catch(...)
    {
    }
    return EINTR;
}



int CFdfs::DownloadFile(const char *pLocalFile, const char *blockId, s64_t *pFileSize )
{
    CDownloadParam downloadParam(pLocalFile, blockId);

    return DownloadCommon(downloadParam, pFileSize);
}

int CFdfs::DownloadFilePart(const char *pLocalFile, const s64_t offset,
                            const char *blockId,    s64_t *pFileSize)
{
    CDownloadParam downloadParam(pLocalFile, offset, blockId);
    return DownloadCommon(downloadParam, pFileSize);
}

int CFdfs::DownloadBuffer(char *pBuffer, const u32_t bufLen,
            const char *blockId, s64_t *pFileSize )
{
    CDownloadParam downloadParam(pBuffer, bufLen, blockId);

    return DownloadCommon(downloadParam, pFileSize);
    
}

int CFdfs::UploadPartFileCb(void *arg, const int64_t file_size, int sock)
{
    if (arg == NULL)
    {
        return EINVAL;
    }

    UploadParam &uploadParam = *(UploadParam *)arg;

    FILE *fd = fopen(uploadParam.mLocalFile, "r");
    if (fd == NULL)
    {
        LOG_ERROR("Failed to fopen %s, rc:%d:%s", 
            uploadParam.mLocalFile, errno, strerror(errno));
        return errno;
    }

    if (fseek(fd, uploadParam.mOffset, SEEK_SET) < 0)
    {
        LOG_ERROR("Failed to fseek %s to pos %llu, rc:%d:%s",
            uploadParam.mLocalFile, uploadParam.mOffset, 
            errno, strerror(errno));
        fclose(fd);
        return errno;
    }

    char *buffer = new char[ONE_MILLION];
    if (buffer == NULL)
    {
        LOG_ERROR("Failed to alloc memory for temp buffer for %s",
            uploadParam.mLocalFile);
        fclose(fd);
        return ENOMEM;
    }

    int rc = STATUS_SUCCESS;
    int eof = 0;
    u64_t  leftReadBytes = uploadParam.mPartFileLen;
    while(leftReadBytes > 0)
    {
        u64_t oneTimeRead = min((u64_t)ONE_MILLION, leftReadBytes);
        u64_t haveRead    = 0;
        while(haveRead < oneTimeRead)
        {
            size_t readBytes = fread(buffer + haveRead, 1, oneTimeRead - haveRead, fd);
            if (readBytes <= 0)
            {
                rc = ferror(fd);
                eof = feof(fd);
                LOG_ERROR("Failed to read %s, haveRead:%llu, leftReadBytes:%llu, rc:%d:%u",
                    uploadParam.mLocalFile, haveRead, leftReadBytes,
                    rc, strerror(rc));
                break;
            }

            haveRead += readBytes;
        }

        if (rc)
        {
            delete buffer;
            fclose(fd);
            return rc;
        }
        rc = tcpsenddata_nb(sock, buffer, haveRead, DEFAULT_NETWORK_TIMEOUT);
        if (rc)
        {
            delete buffer;
            fclose(fd);
            return rc;
        }

        leftReadBytes -= oneTimeRead;

        if (eof)
        {
            LOG_WARN("Finish reading %s, but set %llu need to be read, have read %llu",
                uploadParam.mLocalFile, uploadParam.mPartFileLen, 
                (uploadParam.mPartFileLen - leftReadBytes));
            break;
        }
        
    }

    delete buffer;
    fclose(fd);
    return STATUS_SUCCESS;
}

int CFdfs::UploadCommon(TrackerServerInfo *pTrackerServer, 
                        TrackerServerInfo *pStorageServer, 
                        int                storePathIndex, 
                        UploadParam       &uploadParam,
                        char              *group, 
                        char              *remoteFile)
{
    int rc = STATUS_SUCCESS;
    if (uploadParam.mUploadType == FDFS_APPEND)
    {
        if (uploadParam.mDataType == FDFS_DATA_FILE)
        {
            const char *pLocalFile = uploadParam.mLocalFile;
            rc = storage_upload_appender_by_filename(pTrackerServer, \
                    pStorageServer, storePathIndex, \
                    pLocalFile, NULL, \
                    NULL, 0, \
                    group, remoteFile);
            if (rc)
            {
                LOG_ERROR("Failed to upload appending file %s, storage: %p:%s, error:%d:%s",
                    pLocalFile, pStorageServer, pStorageServer->ip_addr, 
                    rc, strerror(rc));
                return rc;
            }
            //can't parse appending file's size through blockid, so skip it's size
            UpdatLocalStat(group, 0, 1);
            return STATUS_SUCCESS;
        }
        else if (uploadParam.mDataType == FDFS_DATA_BUFFER)
        {
            const char *pBuffer = uploadParam.mBuffer;
            const u64_t bufLen  = uploadParam.mBufLen;

            rc = storage_upload_appender_by_filebuff( \
                pTrackerServer, pStorageServer, \
                storePathIndex, pBuffer, \
                bufLen, NULL, \
                NULL, 0, \
                group, remoteFile);
            if (rc)
            {
                LOG_ERROR("Failed to upload appending buffer %p:%llu, storage: %p:%s, error:%d:%s",
                    pBuffer, bufLen,
                    pStorageServer, pStorageServer->ip_addr, 
                    rc, strerror(rc));
                return rc;
            }
            
            //can't parse appending file's size through blockid, so skip it's size
            UpdatLocalStat(group, 0, 1);
            return STATUS_SUCCESS;
        }

        //else
        
        LOG_ERROR("Unknow append file type");
        return EINVAL;
    
    }
    else if (uploadParam.mUploadType == FDFS_UPLOAD)
    {
        if (uploadParam.mDataType == FDFS_DATA_FILE)
        {
            const char *pLocalFile = uploadParam.mLocalFile;
            rc = storage_upload_by_filename(pTrackerServer, 
                pStorageServer, storePathIndex, 
                pLocalFile, NULL, 
                NULL, 0, 
                group, remoteFile);
            if(rc)
            {
                LOG_ERROR("Failed to upload %s, storage: %p:%s, error:%d, errmsg:%s",
                    pLocalFile, pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                return rc;
            }

                
            struct stat statBuf;
            
            if (stat(pLocalFile, &statBuf) == 0)
            {
                s64_t size = statBuf.st_size;
                UpdatLocalStat(group, size, 1);
            }
        
            return STATUS_SUCCESS;
        }
        else if (uploadParam.mDataType == FDFS_DATA_BUFFER)
        {
            rc = storage_upload_by_filebuff(pTrackerServer, pStorageServer, \
                storePathIndex, uploadParam.mBuffer, \
                uploadParam.mBufLen, NULL, NULL, 0, \
                group, remoteFile);
            if(rc)
            {
                LOG_ERROR("Failed to upload buffer %p, server %p:%s error:%d, errmsg:%s",
                     uploadParam.mBuffer, pStorageServer, 
                     pStorageServer->ip_addr, rc, strerror(rc));
                return rc;
            }

            UpdatLocalStat(group, uploadParam.mBufLen, 1);
            
            return STATUS_SUCCESS;
        }
        else if (uploadParam.mDataType == FDFS_DATA_PART_FILE)
        {
            rc = storage_upload_by_callback( 
                  pTrackerServer, pStorageServer, 
                  storePathIndex, CFdfs::UploadPartFileCb, 
                  &uploadParam, uploadParam.mPartFileLen, 
                  NULL, NULL, 0, 
                  group, remoteFile);
            if(rc)
            {
                LOG_ERROR("Failed to upload file part %s:%llu:%llu, server %p:%s error:%d, errmsg:%s",
                     uploadParam.mLocalFile, uploadParam.mOffset, uploadParam.mPartFileLen,
                     pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                return rc;
            }

            UpdatLocalStat(group, uploadParam.mPartFileLen, 1);
            
            return STATUS_SUCCESS;
        }
        
        LOG_ERROR("Unknow upload type");
        return EINVAL;
    }

    LOG_ERROR("Unknow type");
    return EINVAL;
}

int CFdfs::UploadCommon(UploadParam &uploadParam, char *blockId)
{
    int rc = STATUS_FDFS_CMD;
    try
    {
        TrackerServerInfo     storageServer;
        TrackerServerInfo    *pStorageServer = NULL;
        int                   storePathIndex = 0;
        char                  group[FDFS_GROUP_NAME_MAX_LEN + 1];
        char                  remoteFile[FILENAME_LENGTH_MAX];

        bool                  shortConn = mShortConn;

        u32_t                 trackerIndex = 0;
        TrackerServerInfo    *pTrackerServer = GetTrackerConn(trackerIndex);

        for (int i = 0; i < CFDFS_RETRY_TIMES; i++)
        {
            if (pTrackerServer == NULL)
            {
                rc = ConnectTracker(&pTrackerServer, trackerIndex);
                if (rc)
                {
                    LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                        trackerIndex, rc, strerror(rc));
                    continue;
                }
            }

            if (shortConn)
            {
                pStorageServer = &storageServer;
                rc = GetStorageServer(pTrackerServer, pStorageServer, &storePathIndex);
                if (rc)
                {
                    LOG_ERROR("Failed to get storage server" );
                    
                    rc = ConnectTracker(&pTrackerServer, trackerIndex);
                    if (rc)
                    {
                        LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                            trackerIndex, rc, strerror(rc));
                    }
                    continue;
                }

                rc = ConnectStorageServer(pStorageServer);
                if (rc)
                {
                    LOG_ERROR("Failed to connect storage server %p:%s, rc %d:%s",
                            pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                    continue;
                }
            }
            else
            {
                rc = GetStorageServer(&pStorageServer, &storePathIndex);
                if (rc)
                {
                    LOG_ERROR("Failed to get storage server");
                    shortConn = true;
                    continue;
                }

                if (pStorageServer->sock < 0)
                {
                    //the sock has been broken
                    rc = ConnectStorageServer(pStorageServer);
                    if (rc)
                    {
                        LOG_ERROR("Failed to connect storage server %p:%s, rc %d:%s",
                            pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                        shortConn = true;
                        continue;
                    }
                }
            }

            memset(group, 0, sizeof(group));
            memset(remoteFile, 0, sizeof(remoteFile));
            
            rc = UploadCommon(pTrackerServer, 
                    pStorageServer, storePathIndex, 
                    uploadParam, group, remoteFile);
            if (rc)
            {
                shortConn = true;
                DisConnectServer(pStorageServer);
                continue;
            }

            //upload success
            if (shortConn)
            {
                DisConnectServer(pStorageServer);
            }

            break;
        }

        if (rc == STATUS_SUCCESS)
        {
            int blockIdLen = snprintf(blockId, FILENAME_LENGTH_MAX, "%s/%s", group, remoteFile);
            if (blockIdLen > 0)
            {
                blockId[blockIdLen] = '\0';
            }
            return rc;
        }
        //Failed
        return rc;

    }
    catch(...)
    {
    }
    return EINTR;
}



int CFdfs::AppendFile(const char *pLocalFile, char *blockId)
{
    UploadParam appendParam(FDFS_APPEND, pLocalFile);
    
    return UploadCommon(appendParam, blockId);
}

int CFdfs::AppendBuffer(const char *pBuffer, s64_t bufLen, char *blockId)
{
    UploadParam appendParam(FDFS_APPEND, pBuffer, bufLen);

    return UploadCommon(appendParam, blockId);
}

int CFdfs::UploadFilePart(const char *pLocalFile, const u64_t offset, 
                          const u64_t len,        char *blockId)
{
    UploadParam uploadParam(FDFS_UPLOAD, pLocalFile, offset, len);

    return UploadCommon(uploadParam, blockId);
}

int CFdfs::UploadFile(const char *pLocalFile, char *blockId)
{
    UploadParam uploadParam(FDFS_UPLOAD, pLocalFile);

    return UploadCommon(uploadParam, blockId);
}

int CFdfs::UploadBuffer(const char *pBuffer, u64_t length, char *blockId)
{
    UploadParam uploadParam(FDFS_UPLOAD, pBuffer, length);

    return UploadCommon(uploadParam, blockId);
}

int CFdfs::AppendBufferToFile(const char *pBuffer, s64_t bufLen, const char *blockId)
{
    int rc = STATUS_FDFS_CMD;
    try
    {
        FDFS_SPLIT_GROUP_NAME_AND_FILENAME(blockId);

        bool  shortConn = mShortConn;

        u32_t                trackerIndex   = 0;
        TrackerServerInfo   *pTrackerServer = GetTrackerConn(trackerIndex);
        
        for (int i = 0; i < CFDFS_RETRY_TIMES; i++)
        {
            if (pTrackerServer == NULL)
            {
                rc = ConnectTracker(&pTrackerServer, trackerIndex);
                if (rc)
                {
                    LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                        trackerIndex, rc, strerror(rc));
                    continue;
                }
            }
            
            TrackerServerInfo *pStorageServer = NULL;
            if (shortConn == false)
            {
                rc = GetStorageByBlockid(blockId, &pStorageServer);
                if (rc)
                {
                    LOG_WARN("Failed to get StorageInfo from blockId %s", blockId);
                    shortConn = true;
                    continue;
                }

                if (pStorageServer->sock < 0)
                {
                    //the sock has been broken
                    rc = ConnectStorageServer(pStorageServer);
                    if (rc)
                    {
                        LOG_ERROR("Failed to connect storage server %p:%s, rc %d:%s",
                            pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                        shortConn = true;
                        continue;
                    }
                }
                
            }

            rc = storage_append_by_filebuff(pTrackerServer, \
                pStorageServer, pBuffer, \
                (int64_t)bufLen, group_name, \
                filename);
            if(rc)
            {
                LOG_ERROR("Failed to append %s/%s from buffer %p:%lld, server %p, error:%d:%s",
                    group_name, filename, pBuffer, bufLen, 
                    pStorageServer, rc, strerror(rc));
                if (shortConn)
                {
                    int tempRc = ConnectTracker(&pTrackerServer, trackerIndex);
                    if (tempRc)
                    {
                        LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                            trackerIndex, tempRc, strerror(tempRc));
                    }
                    
                }
                else
                {
                    DisConnectServer(pStorageServer);
                }
                shortConn = true;
                continue;
            }
            else
            {
                
                UpdatLocalStat(group_name, 0, 0);
                return STATUS_SUCCESS;
            }

            
        }
        return rc;
    }
    catch(...)
    {
    }
    return EINTR;
}

int CFdfs::DeleteFile(const char *blockId, s64_t  &fileSize)
{
    int rc = STATUS_FDFS_CMD;
    try
    {
        FDFS_SPLIT_GROUP_NAME_AND_FILENAME(blockId);
        GetFileSize(blockId, fileSize);
        if (fileSize == INFINITE_FILE_SIZE)
        {
            //Can't parse appending file's size through blockId, so skip it
            fileSize = 0;
        }

        bool  shortConn = mShortConn;

        u32_t                trackerIndex   = 0;
        TrackerServerInfo   *pTrackerServer = GetTrackerConn(trackerIndex);
        
        for (int i = 0; i < CFDFS_RETRY_TIMES; i++)
        {
            if (pTrackerServer == NULL)
            {
                rc = ConnectTracker(&pTrackerServer, trackerIndex);
                if (rc)
                {
                    LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                        trackerIndex, rc, strerror(rc));
                    continue;
                }
            }
            
            TrackerServerInfo *pStorageServer = NULL;
            if (shortConn == false)
            {
                rc = GetStorageByBlockid(blockId, &pStorageServer);
                if (rc)
                {
                    LOG_WARN("Failed to get StorageInfo from blockId %s", blockId);
                    shortConn = true;
                    continue;
                }

                if (pStorageServer->sock < 0)
                {
                    //the sock has been broken
                    rc = ConnectStorageServer(pStorageServer);
                    if (rc)
                    {
                        LOG_ERROR("Failed to connect storage server %p:%s, rc %d:%s",
                            pStorageServer, pStorageServer->ip_addr, rc, strerror(rc));
                        shortConn = true;
                        continue;
                    }
                }
                
            }

            // if pStorageInfo == NULL, storage_delete_file will do query automatically
            rc = storage_delete_file(pTrackerServer, pStorageServer, group_name, filename);
            if(rc)
            {
                LOG_ERROR("Failed to DeleteFile %s, server:%p, error:%d, errmsg:%s",
                    blockId, pStorageServer, rc, strerror(rc));
                if (shortConn)
                {
                    int tempRc = ConnectTracker(&pTrackerServer, trackerIndex);
                    if (tempRc)
                    {
                        LOG_ERROR("Failed to connect tracker, index:%u, rc:%d:%s", 
                            trackerIndex, tempRc, strerror(tempRc));
                    }
                }
                else
                {
                    DisConnectServer(pStorageServer);
                }
                shortConn = true;
                continue;
            }
            else
            {
                //Success
                UpdatLocalStat(group_name, -fileSize, -1);
                break;
            }
            
        }
        return rc;
    }
    catch(...)
    {
    }
    return EINTR;
}



int CFdfs::GetFileSize(const char *blockId, s64_t &fsSize)
{    
    int ret = 0;

    FDFSFileInfo fsInfo;

    for (int i = 0; i < CFDFS_RETRY_TIMES; i++)
    {
        ret = fdfs_get_file_info1(blockId, &fsInfo);
        if (ret)
        {
            LOG_WARN("Failed to parse blockId to get filesize:%s", blockId);
            continue;
        }
        else
        {
            break;
        }
    }

    if (ret)
    {
        return ret;
    }

    fsSize = fsInfo.file_size;

    return 0;
}

int CFdfs::GetStorageByBlockid(const char *blockId, TrackerServerInfo **ppStorageInfo)
{
    int ret = 0;

    FDFSFileInfo fsInfo;

    for (int i = 0; i < CFDFS_RETRY_TIMES; i++)
    {
        ret = fdfs_get_file_info1(blockId, &fsInfo);
        if (ret)
        {
            LOG_WARN("Failed to parse blockId to get filesize:%s", blockId);
            continue;
        }
        else
        {
            break;
        }
    }

    if (ret)
    {
        return ret;
    }

    FDFS_SPLIT_GROUP_NAME_AND_FILENAME(blockId);
    std::map<std::string, GroupInfo *>::iterator it = mGroupMap.find(group_name);

    if (it == mGroupMap.end() || it->second == NULL)
    {
        LOG_WARN("Failed to find GroupInfo in mGroupMap of %s", group_name);
        return ENOENT;
    }

    GroupInfo *pGroupInfo = it->second;

    std::string server; 
    if (GetHostname(fsInfo.source_ip_addr, server))
    {
        server = fsInfo.source_ip_addr;
    }

    std::map<std::string, ServerInfo *>::iterator itServer = pGroupInfo->mServerMap.find(server);
    if (itServer == pGroupInfo->mServerMap.end())
    {
        LOG_WARN("Failed to find ServerInfo %s in %s", server.c_str(), group_name);
        return ENOENT;
    }

    ServerInfo *pServerInfo = itServer->second;
    *ppStorageInfo = pServerInfo->GetServerConn();

    return STATUS_SUCCESS;
    
}

int   CFdfs::GetMetricsChange(s64_t blockChange, s64_t sizeChange)
{
    MUTEX_LOCK(&mLock);

    blockChange  = mBlockChange;
    mBlockChange = 0;

    sizeChange   = mSizeChange;
    mSizeChange  = 0;

    MUTEX_UNLOCK(&mLock);

    return STATUS_SUCCESS;
}

void CFdfs::UpdatLocalStat(const char * groupName, const s64_t sizeChange, const int nodeChange)
{
    MUTEX_LOCK(&mLock);
    
    std::map<std::string, GroupInfo *>::iterator it = mGroupMap.find(groupName);
    if (it != mGroupMap.end())
    {
        GroupInfo *pGroupInfo = it->second;
        pGroupInfo->mOpCount++;
        pGroupInfo->mFreeCapacity -= sizeChange;
    }

    mBlockChange += nodeChange;

    mSizeChange  += sizeChange;

    MUTEX_UNLOCK(&mLock);
}

int CFdfs::Test()
{
    int ret = STATUS_SUCCESS;

    //upload configure file to server
    char blockId[FILENAME_LENGTH_MAX] = {0};
    ret = UploadFile(mDFSCfg.c_str(), blockId);
    if (ret)
    {
        LOG_ERROR("Failed to upload file");
        return ret;
    }

    char localFile[FILENAME_LENGTH_MAX] = {0};
    time_t now;
    time(&now);
    snprintf(localFile, sizeof(localFile), "/tmp/.%lld", (long long)now);
    s64_t fileSize = 0;
    ret = DownloadFile(localFile, blockId, &fileSize);
    if (ret)
    {
        LOG_ERROR("Failed to download %s to %s", blockId, localFile);
        return ret;
    }

    s64_t fileSize2;
    ret = DeleteFile(blockId, fileSize2);
    if (ret)
    {
        LOG_ERROR("Failed to delete %s", blockId);
        return ret;
    }
    if (fileSize != fileSize2)
    {
        LOG_ERROR("Delete file size %u is not equal as download %u",
            (u32_t)fileSize2, (u32_t)fileSize);
        return STATUS_UNKNOW_ERROR;
    }

    ret = DownloadFile(localFile, blockId, &fileSize);
    if (ret != ENOENT)
    {
        LOG_ERROR("%s still exist in server", blockId);
        return STATUS_UNKNOW_ERROR;
    }
    unlink(localFile);

    std::string srcData;
    ret = ReadFromFile(mDFSCfg, srcData);
    if (ret)
    {
        LOG_ERROR("Failed to read from file %s", mDFSCfg.c_str());
        return ret;
    }

    ret = UploadBuffer(srcData.c_str(), srcData.size(), blockId);
    if (ret)
    {
        LOG_ERROR("Failed to upload buffer %s's content to DFS", mDFSCfg.c_str());
        return ret;
    }

    char *buffer = new char[fileSize2 + 1];
    if (buffer == NULL)
    {
        LOG_ERROR("Failed to alloc memory for buffer");
        return ENOMEM;
        
    }

    s64_t bufferSize = 0;
    ret = DownloadBuffer(buffer, fileSize2 + 1, blockId, &bufferSize);
    if (ret)
    {
        LOG_ERROR("Failed to download buffer %s", blockId);
        delete buffer;
        return ret;
    }

    if (bufferSize != fileSize2 )
    {
        LOG_ERROR("buffer size not equal");
        delete buffer;
        return ret;
    }

    for (int i = 0; i < fileSize2; i++)
    {
        if (buffer[i] != srcData[i])
        {
            LOG_ERROR("Data is not equal");
            break;
        }
    }

    ret = DeleteFile(blockId, fileSize2);
    if (ret)
    {
        LOG_ERROR("Failed to delete %s", blockId);
        return ret;
    }

    ret = AppendFile(mDFSCfg.c_str(), blockId);
    if (ret)
    {
        LOG_ERROR("Failed to append %s to %s", mDFSCfg.c_str(), blockId);
        return ret;
    }

    ret = AppendBufferToFile(srcData.c_str(), srcData.size(), blockId);
    if (ret)
    {
        LOG_ERROR("Failed to append buffer %s's content to %s",
            mDFSCfg.c_str(), blockId);
        return ret;
    }

    buffer = (char *)realloc(buffer, fileSize2 + srcData.size() + 1);
    if (buffer == NULL)
    {
        LOG_ERROR("No memory to store appender file's buffer");
        return ENOMEM;
    }
    
    ret = DownloadBuffer(buffer, fileSize2 + srcData.size() + 1, blockId, &bufferSize);
    if (ret)
    {
        LOG_ERROR("Failed to download buffer %s", blockId);
        delete buffer;
        return ret;
    }

    if (bufferSize != fileSize2 + srcData.size() )
    {
        LOG_ERROR("buffer size not equal");
        delete buffer;
        return ret;
    }

    for (int i = 0; i < srcData.size(); i++)
    {
        if (buffer[i + fileSize2] != srcData[i])
        {
            LOG_ERROR("Data is not equal");
            break;
        }
    }

    ret = DeleteFile(blockId, fileSize2);
    if (ret)
    {
        LOG_ERROR("Failed to delete %s", blockId);
        return ret;
    }

    delete buffer;

    ret = AppendBuffer(srcData.c_str(), srcData.size(), blockId);
    if (ret)
    {
        LOG_ERROR("Failed to append buffer %s's",
            mDFSCfg.c_str());
        return ret;
    }

    ret = DeleteFile(blockId, fileSize);
    if (ret)
    {
        LOG_ERROR("Failed to delete file %s", blockId);
        return ret;
    }

    
    return STATUS_SUCCESS;
}

CFdfs &GlobalDfsInstance()
{
    static CFdfs gFdfs;

    return gFdfs;
}

int InitDfs()
{
    int rc = STATUS_SUCCESS;

    rc = GlobalDfsInstance().InitDFSConnection();
    if (rc)
    {
        LOG_ERROR("Failed to init default DFS instance");
        return rc;
    }

    GlobalDfsInstance().Test();

#if 0
    int i = 0;
    int testRc = 0;
    while(i < 3000)
    {
        int testRc = CFdfs::GetDfsInstance().Test();
        if (testRc)
        {
            break;
        }
        else
        {
            i++;
            LOG_INFO("Success %d", i); 
        }
    }

    if (testRc)
    {
        LOG_ERROR("Something is wrong :retry:%d:%d:%s", i, testRc, strerror(testRc));
    }
#endif

    LOG_INFO("Successfully init DFS");
    return STATUS_SUCCESS;
}

