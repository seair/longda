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
 * date:   2011/04/01
 * func:   Encapsulate fdfs client API
 */ 

#ifndef __CFDFS_H__

#define __CFDFS_H__
//********************************************************************
#include <string>
#include <iostream>
#include <map>

#include "fastcommon/base64.h"
#include "fastdfs/fdfs_client.h"
#include "fastdfs/fdfs_global.h"
#include "fastdfs/fdfs_http_shared.h"

#include "defs.h"
#include "mutex.h"

class ServerInfo
{
public:
    ServerInfo(const FDFSStorageInfo    &storageStat, const char *groupName, const u32_t maxConnNum);
    ~ServerInfo();

    u32_t GetNextDisk();
    TrackerServerInfo* GetServerConn();
    void  ResetStat(const FDFSStorageInfo    &storageStat);

public:
    pthread_mutex_t    mLock;
    u32_t              mMaxConnNum;
    TrackerServerInfo  *mConnections;
    
    u32_t              mTotalDiskNum;
    FDFSStorageInfo    mStat;
    
};
//define 
class GroupInfo
{
public:
    GroupInfo();
    ~GroupInfo();

    void UpdateStat();
    int  GetStorageServer(TrackerServerInfo ** ppStorageServer, int * pStorePathIndex);
public:
    u64_t               mOpCount;
    u64_t               mFreeCapacity;               //unit is MB
    u32_t               mCurServerIndex;
    std::map<std::string, ServerInfo *> mServerMap;
    FDFSGroupStat        mGroupStat;
};



class CFdfs
{
public:
    CFdfs();
    ~CFdfs();
    void Destory();

    /*  
     * func:      init DFS client
     * @param[in] szCfgFile       fastdfs client configure file
     * notes: Haven't check pLocalFile, make sure szCfgFile is valid
     */
    int InitDFSConnection();
    /*  
     * func:      download blockId from servers to pLocalFile 
     * @param[in] pLocalFile       local file to store remote data
     * @param[in] blockId           group/remoteFile
     * @param[out] pFileSize       file size
     * notes: Haven't check pLocalFile, make sure pLocalFile is valid
     */
    int DownloadFile(const char *pLocalFile, const char *blockId, s64_t *pFileSize);

    int  DownloadBuffer(char *pBuffer, const u32_t bufLen,
            const char *blockId, s64_t *pFileSize);

    int DownloadFilePart(const char *pLocalFile, const s64_t offset,
                         const char *blockId,    s64_t *pFileSize);

    /*  
     * func:      upload local file/buffer to DFS, but the uploaded file can be appended 
     * @param[in] pLocalFile        local file to store remote data
     * @param[out] blockId          group/remoteFile
     * notes: Haven't check pLocalFile, make sure pLocalFile is valid
     */
    int AppendFile(const char *pLocalFile, char *blockId);
    int AppendBuffer(const char *pBuffer, s64_t bufLen, char *blockId);

    /*  
     * func:      append buffer to blockId file 
     * @param[in] pBuffer           buffer to store remote data
     * @param[in] bufLen            buffer size
     * @param[in] blockId           group/remoteFile
     * notes: Haven't check pLocalFile, make sure pLocalFile is valid
     */
    int AppendBufferToFile(const char *pBuffer, s64_t bufLen, const char *blockId);
    
    /*  
     * func:      upload pLocalFile to servers 
     * @param[in] pLocalFile       local file to upload
     * @param[out] blockId           group/remoteFile
     * notes: Haven't check pLocalFile, make sure pLocalFile is valid
     */
    int UploadFile(const char *pLocalFile, char *blockId);

    int UploadBuffer(const char *pBuffer, u64_t length, char *blockId);

    int UploadFilePart(const char *pLocalFile, const u64_t offset, 
                       const u64_t len,        char *blockId);
    
    /*  
     * func:      Delete file in server
     * @param[in] blockId           group/remoteFile
     */
    int DeleteFile(const char *blockId, s64_t  &fileSize);

    /*
     * func:      Get file size
     * @param[in] blockId            group/remoteFile
     * @param[out] fsSize
     */
    int GetFileSize(const char *blockId, long long &fsSize);

    /*  
     * func:      Test all api is ok or not
     */
    int  Test();

    u64_t GetFreeCapacity();

    int   GetMetricsChange(s64_t blockChange, s64_t sizeChange);
    
    
protected:

    int  InitLog();

    int SetProperies();

    TrackerServerInfo* GetTrackerConn(u32_t &trackerIndex);

    void SetTrackerConn(TrackerServerInfo *pTrackerServer, u32_t index);

    int ConnectTracker(TrackerServerInfo  **ppTrackerServer, const u32_t trackerIndex);
    /*  
     * func:      query storage server from tracker, if group haven't been set, 
     *            then query from all storage servers
     * @param[out] pStorageServer           storage server
     * @param[out] pStoragePathIndex        disk index
     * @param[in]  szGroupName              storage group name, default is empty
     */
    int GetStorageServer(TrackerServerInfo  *pTrackerServer,
                            TrackerServerInfo  *pStorageServer, 
                            int * pStorePathIndex, const std::string szGroupName = "");

    /*  
     * func:      query storage server by myself, if group haven't been set, 
     *            then query from all storage servers
     * @param[out] ppStorageServer           storage server
     * @param[out] pStoragePathIndex        disk index
     * @param[in]  szGroupName              storage group name, default is empty
     */
    int GetStorageServer(TrackerServerInfo ** ppStorageServer, int * pStorePathIndex, 
        const  std::string szGroupName = "");
    
    int ConnectStorageServer(TrackerServerInfo *pStorageServer);
    int DisConnectServer(TrackerServerInfo *pStorageServer);
   

    int  GetStorageByBlockid(const char *blockId, TrackerServerInfo **ppStorageInfo);

    void UpdatLocalStat(const char * groupName, const s64_t sizeChange, const int nodeChange);
    /*
     * func:      Get all group information
     * return     0 success, other failure
     */
    int  GetAllGroups();

    int  GetAllServers();

    static int UploadPartFileCb(void *arg, const int64_t file_size, int sock);

    
    
private:
    

    typedef enum {
        ROLL             = 0,
        OPERATION_NUM,
        CAPACITY,
        UPLOAD_END
    }UPLOAD_TYPE;

    typedef enum {
        FDFS_DATA_FILE     = 0,
        FDFS_DATA_PART_FILE,
        FDFS_DATA_BUFFER
    }FdfsDataType;

    typedef enum {
        FDFS_UPLOAD       = 0,
        FDFS_APPEND  
    }FdfsUploadType;

    class CDownloadParam
    {
    public:
        CDownloadParam(const char *pLocalFile, const char *blockId):
            mType(FDFS_DATA_FILE),
            mBlockId(blockId),
            mLocalFile(pLocalFile),
            mOffset(0),
            mBuffer(NULL),
            mBufLen(0)
        {
        }
            
        CDownloadParam(const char *pLocalFile, const u64_t offset, const char *blockId):
            mType(FDFS_DATA_PART_FILE),
            mBlockId(blockId),
            mLocalFile(pLocalFile),
            mOffset(offset),
            mBuffer(NULL),
            mBufLen(0)
        {
        }
            
        CDownloadParam(char *pBuffer, const u32_t bufLen, const char *blockId):
            mType(FDFS_DATA_BUFFER),
            mBlockId(blockId),
            mLocalFile(NULL),
            mOffset(0),
            mBuffer(pBuffer),
            mBufLen(bufLen)
        {
        }

    public:
        FdfsDataType   mType;
        const char    *mBlockId;
        const char    *mLocalFile;
        const u64_t    mOffset;
              char    *mBuffer;
        const u32_t    mBufLen;
    };

    class UploadParam
    {
    public:
        UploadParam(FdfsUploadType uploadType, const char *pLocalFile):
            mUploadType(uploadType),
            mDataType(FDFS_DATA_FILE),
            mLocalFile(pLocalFile),
            mBuffer(NULL),
            mBufLen(0),
            mOffset(0),
            mPartFileLen(0)
        {
        }

        UploadParam(FdfsUploadType uploadType, const char *pLocalFile, 
                    const u64_t offset,        const u64_t len):
            mUploadType(uploadType),
            mDataType(FDFS_DATA_PART_FILE),
            mLocalFile(pLocalFile),
            mBuffer(NULL),
            mBufLen(0),
            mOffset(offset),
            mPartFileLen(len)
        {
        }

        UploadParam(FdfsUploadType uploadType, const char *pBuffer, u64_t length):
            mUploadType(uploadType),
            mDataType(FDFS_DATA_BUFFER),
            mLocalFile(NULL),
            mBuffer(pBuffer),
            mBufLen(length),
            mOffset(0),
            mPartFileLen(0)
        {
        }

    public:
        FdfsUploadType         mUploadType;
        FdfsDataType           mDataType;
        const char            *mLocalFile;
        const char            *mBuffer;
        const u64_t            mBufLen;
        const u64_t            mOffset;
        const u64_t            mPartFileLen;
    };

protected:
    
    int UploadCommon(UploadParam &uploadParam, char *blockId);
    int UploadCommon(TrackerServerInfo *pTrackerServer, 
                     TrackerServerInfo *pStorageServer, 
                     int                storePathIndex, 
                     UploadParam       &uploadParam,
                     char              *group, 
                     char              *remoteFile);

    int DownloadCommon(CDownloadParam &downloadParam, s64_t *pFileSize);
    int DownloadCommon(TrackerServerInfo   *pTrackerServer, 
                       TrackerServerInfo   *pStorageServer, 
                       const char          *group_name, 
                       const char          *filename, 
                       CDownloadParam       &downloadParam, 
                       s64_t                *pFileSize);

private:
    std::string           mDFSCfg;
    std::string           mLogFile;
    

    //map groupname <--> GroupInfo
    std::map<std::string, GroupInfo *>mGroupMap;
    u32_t                 mCurGroupIndex;
    
    bool                  mInit;

    UPLOAD_TYPE           mUploadType;
    bool                  mShortConn;

    TrackerServerInfo   **mTrackerServers;
    TrackerServerGroup   *mTrackerGroups;
    u32_t                 mMaxConnNum;
    pthread_mutex_t       mLock;

    s64_t                 mBlockChange;
    s64_t                 mSizeChange;

};


CFdfs &GlobalDfsInstance();
int    InitDfs();

#endif //__CFDFS_H__
