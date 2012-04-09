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
 * date:   2011/05/27
 * func:   define metadata
 */ 

#ifndef __METADATA_H__
#define __METADATA_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <string.h>

#include "defs.h"

#define OBJID_LEN        33
#define BLOCKID_LEN      128


#define ADFS_DEFAULT_BLOCK_SIZE              0x1000000

#define ADFS_DATA_GEOG_SIMPLE                0x00000000
#define ADFS_DATA_GEOG_EXTENSION             0x00000001

#define FILE_OP_MODE            (S_IRWXU|S_IRWXG|S_IRWXO)
#define DEFAULT_OP_MODE         (S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)
#define DEFAULT_DIR_MODE        (S_IFDIR|DEFAULT_OP_MODE)
#define DEFAULT_FILE_MODE       (DEFAULT_OP_MODE)
#define DEFAULT_SYSLINK_MODE    (S_IFLNK|DEFAULT_OP_MODE)

#define ROOT_PATH               "/"
#define PATH_SEP                '/'
#define MAGIC_DEV               ('A' << 24 | 'D' << 16 | 'F' << 8 | 'S')

const unsigned int BLOCK_SECTOR_SIZE   = 512;
const char DHT_FSMETRICS_OID[]         = "FS_METRICS_OBJID";

#define NO_METADATA_OBJ         ENOENT        


typedef struct _treeNode {
    char       mParentId[OBJID_LEN];
}TreeNode;

typedef struct _symLinkNode {
    char        mSource[FILENAME_LENGTH_MAX];
}SymLinkNode;

typedef struct _hardLinkNode {
    char       mLeftId[OBJID_LEN];
    char       mRightId[OBJID_LEN];
}HardLinkNode;

typedef struct _adfsNode {
    TreeNode     mTreeNode;
    SymLinkNode  mSysLink;
}AdfsNode;

typedef struct _metaData{
    char        mObjId[OBJID_LEN];
    /**
     * the link in stat is useless
     */
    struct stat mStat;
    u32_t       mExtension;
    union{
        char        mMaxIdValue[BLOCKID_LEN];       // this is the max length
        char        mBlockId[BLOCKID_LEN];
        char        mDGOID[OBJID_LEN];
    };
    char        mFileName[FILENAME_LENGTH_MAX];
    AdfsNode    mAdfsNode;
}MetaData;

typedef struct _metaDataStore
{
    u32_t         mVersion;
    union {
        char      mValue[ONE_KILO];
        MetaData  mMetaData;
        
    };
}MetaDataStore;

typedef struct _fsMetric
{
    struct statvfs mFsStat;
}FsMetric;

typedef struct _fsMetricStore
{
    u32_t        mVersion;
    union {
        char     mValue[ONE_KILO];
        FsMetric mFsMetric;
    };
}FsMetricStore;

typedef struct _blockGeography
{
    u64_t       mOffset;
    u64_t       mLen;
    u64_t       mStatus;
    char        mBlockId[BLOCKID_LEN];
}BlockGeography;

typedef struct _dataGeography
{
    u32_t            mVersion;
    u32_t            mHardLinkNum;
    u32_t            mBlockNum;
    BlockGeography   mBlocks[0];
}DataGeography;

inline void InitDataGeography(DataGeography &dg, u32_t blockNum)
{
    dg.mVersion     = VERSION_NUM;
    dg.mHardLinkNum = 1;
    dg.mBlockNum    = blockNum;
    memset(dg.mBlocks, 0, sizeof(BlockGeography) * blockNum);
}


typedef struct _dirEntry
{
    char       mName[FILENAME_LENGTH_MAX];
    u64_t      mInode;
    u64_t      mMode;
}DirEntry;


#endif //__METADATA_H__
