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
 * func:   fuse entrance
 */

#include "log.h"
#include "util.h"
#include "md5.h"
#include "fdfs.h"
#include "fdht.h"

#include "fusentry.h"
#include "afuseinit.h"

#include "clientevent.h"

#define REVERT_METADATA_TIMES     3
#define UPLOAD_ON_CLOSE           0

void FreeMountArgs(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++)
    {
        free(argv[i]);
        argv[i] = NULL;
    }

    return;
}

int InitMountArgs(int &argc, char *argv[])
{
    argc = 0;
    argv[argc++] = strdup(GlobalConfigPath()->mProcessName.c_str());

    std::string key;
    std::string value;

    key = "MOUNT_DIR";
    value = theGlobalProperties()->GetParamValue(key, DEFAULT_SESSION);
    if (value.empty())
    {
        LOG_ERROR("%s hasn't been set", key.c_str());
        FreeMountArgs(argc, argv);
        return STATUS_INIT_FUSE;
    }
    argv[argc++] = strdup(value.c_str());

    if (GlobalConfigPath()->mDemon == false)
    {
        argv[argc++] = strdup("-f");
    }

    key = "MOUNT_ARGS";
    value = theGlobalProperties()->GetParamValue(key, DEFAULT_SESSION);
    if (value.empty())
    {
        LOG_ERROR("%s hasn't been set", key.c_str());
        FreeMountArgs(argc, argv);
        return STATUS_INIT_FUSE;
    }
    value = value.substr(1, value.size() - 2);

    std::vector < std::string > valueList;
    Xlate::SplitString(value, " ", valueList);
    for (std::vector<std::string>::iterator it = valueList.begin();
            it != valueList.end(); it++)
    {
        std::string &arg = *it;
        argv[argc++] = strdup(arg.c_str());
    }

    for (int i = 0; i < argc; i++)
    {
        LOG_INFO("Mount args %d:%s", i, argv[i]);
    }

    return STATUS_SUCCESS;
}

std::string CFuse::sTempDir;
struct fuse_operations CFuse::adfs_oper;
bool CFuse::mWriteThrough;

void CFuse::ConvertObjId(const char *path, char *objId)
{
    char md5Digest[16] = { 0 };
    MD5String(path, (unsigned char *) md5Digest);
    Bin2Hex(md5Digest, sizeof(md5Digest), objId);
}

void CFuse::InitMetaData(MetaDataStore &metaDataStore, mode_t st_mode,
        const char *path)
{
    metaDataStore.mVersion = VERSION_NUM;
    MetaData &metaData = metaDataStore.mMetaData;

    ConvertObjId(path, metaData.mObjId);
    metaData.mStat.st_dev = MAGIC_DEV;
    metaData.mStat.st_mode = st_mode;
    metaData.mStat.st_blksize = BLOCK_SECTOR_SIZE;
    if (st_mode & S_IFDIR)
    {
        metaData.mStat.st_size = 4 * ONE_KILO;
        metaData.mStat.st_blocks = metaData.mStat.st_size / BLOCK_SECTOR_SIZE;
        metaData.mStat.st_nlink = 2;

    } else if (S_ISREG(st_mode))
    {
        metaData.mStat.st_nlink = 1;
    } else if (S_ISLNK(st_mode))
    {
        metaData.mStat.st_nlink = 1;
    }

    metaData.mStat.st_uid = getuid();
    metaData.mStat.st_gid = getgid();

    time_t now;
    time(&now);
    metaData.mStat.st_atime = now;
    metaData.mStat.st_mtime = now;
    metaData.mStat.st_ctime = now;

    std::string fileName;
    GetFileName(path, fileName);
    strncpy(metaData.mFileName, fileName.c_str(),
            sizeof(metaData.mFileName) - 1);

}

int CFuse::DelMetaData(MetaDataStore &metaDataStore)
{
    int res = GlobalMetaCache()->DelKeyV(metaDataStore.mMetaData.mObjId);
    if (res)
    {
        LOG_ERROR("Failed to delete %s metadat, res:%d:%s",
                metaDataStore.mMetaData.mObjId, res, strerror(res));
        return res;
    }
    return STATUS_SUCCESS;
}

int CFuse::SetMetaData(MetaDataStore &metaDataStore)
{
    int res = GlobalMetaCache()->SaveKeyV(metaDataStore.mMetaData.mObjId,
            (char *) &metaDataStore, sizeof(metaDataStore));
    if (res)
    {
        LOG_ERROR("Failed to set %s metadat: res:%d:%s",
                metaDataStore.mMetaData.mObjId, res, strerror(res));
        return res;
    }
    return STATUS_SUCCESS;
}

int CFuse::GetMetaDataByID(const char *objId, MetaDataStore **metaData,
        MetaOp op)
{
    ASSERT(metaData, "invalid metaData parameter");

    int res = STATUS_SUCCESS;

    char *dhtValue = NULL;
    int dhtValueLen = 0;
    res = GlobalMetaCache()->GetKeyV(objId, &dhtValue, &dhtValueLen, op);
    if (res)
    {
        if (res == ENOENT)
        {
            LOG_DEBUG("NO metadat: objId:%s, res:%d:%s",
                    objId, res, strerror(res));
        } else
        {
            LOG_ERROR("Failed to get metadat: objId:%s, res:%d:%s",
                    objId, res, strerror(res));
        }

        return res;
    }

    *metaData = (MetaDataStore *) dhtValue;
    return STATUS_SUCCESS;
}

/*
 * make sure the fileName of the path is less 256
 */
int CFuse::GetMetaDataByPath(const char *path, MetaDataStore **metaData,
        MetaOp op)
{

    ASSERT(metaData, "invalid metaData parameter");

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    return GetMetaDataByID(objId, metaData, op);
}

int CFuse::AllocMetaData(const char *objId, MetaDataStore **metaData)
{
    ASSERT(metaData, "invalid metaData parameter");

    return GlobalMetaCache()->AllocKeyV(objId, (char **) metaData,
            (int) sizeof(MetaDataStore));
}

void CFuse::FreeMetaData(MetaDataStore **metaData, bool forceFree, MetaOp op)
{
    ASSERT(metaData, "invalid metaData parameter");

    if (metaData && *metaData)
    {
        int ret = GlobalMetaCache()->PutKeyV((*metaData)->mMetaData.mObjId,
                forceFree, op);
        if (ret)
        {
            //cache layer free failed
            free(*metaData);
        }

        *metaData = NULL;
    }
}

void CFuse::CopyMetaData(MetaDataStore &target, const MetaDataStore &source)
{
    MetaData &metaData = target.mMetaData;

    metaData.mStat = source.mMetaData.mStat;

    metaData.mExtension = source.mMetaData.mExtension;
    strcpy(metaData.mMaxIdValue, source.mMetaData.mMaxIdValue);

    time_t now;
    time(&now);
    target.mMetaData.mStat.st_atime = now;
    target.mMetaData.mStat.st_mtime = now;
    target.mMetaData.mStat.st_ctime = now;

    return;
}

int CFuse::GenerateTempFile(const char *objId, char *tempName)
{
    const std::string &nameSpace = GlobalMetaCache()->GetDHTNameSpace();

    snprintf(tempName, FILENAME_LENGTH_MAX - 1, "%s/%s_%s", sTempDir.c_str(),
            nameSpace.c_str(), objId);

    return STATUS_SUCCESS;
}

int CFuse::InsertTreeMeta(MetaDataStore &metaDataStore, const char *parentOID)
{
    if (parentOID == NULL || strlen(parentOID) == 0)
    {
        return ENOENT;
    }

    int ret = 0;

    strcpy(metaDataStore.mMetaData.mAdfsNode.mTreeNode.mParentId, parentOID);
    ret = SetMetaData(metaDataStore);
    if (ret)
    {
        LOG_ERROR(
                "Failed to SetMetaData %s:%s, rc:%d:%s",
                metaDataStore.mMetaData.mFileName, metaDataStore.mMetaData.mObjId, ret, strerror(ret));
        return ret;
    }

    MetaDataStore *parentMeta = NULL;
    ret = GetMetaDataByID(parentOID, &parentMeta);
    if (ret)
    {
        LOG_ERROR("Failed to get parent metadata, parentOID:%s, rc:%d:%s",
                parentOID, ret, strerror(ret));

        DelMetaData(metaDataStore);

        return ret;
    }

    if (!(parentMeta->mMetaData.mStat.st_mode & S_IFDIR))
    {
        LOG_ERROR(
                "%s Parent %s isn't a directory ",
                metaDataStore.mMetaData.mFileName, parentMeta->mMetaData.mFileName);

        FreeMetaData(&parentMeta);

        DelMetaData(metaDataStore);
        return EINVAL;

    }

    DirEntry dirEntry;
    memset(&dirEntry, 0, sizeof(dirEntry));
    strncpy(dirEntry.mName, metaDataStore.mMetaData.mFileName,
            FILENAME_LENGTH_MAX - 1);
    dirEntry.mInode = metaDataStore.mMetaData.mStat.st_ino;
    dirEntry.mMode = metaDataStore.mMetaData.mStat.st_mode;

    char uploadBuf[ONE_KILO] = { 0 };
    memcpy(uploadBuf, &dirEntry, sizeof(dirEntry));
    uploadBuf[sizeof(dirEntry)] = '\n';

    s64_t uploadBufLen = sizeof(dirEntry) + 1;

    char *parentBlockId = parentMeta->mMetaData.mBlockId;
    char *parentName = parentMeta->mMetaData.mFileName;

    if (strlen(parentBlockId))
    {
        ret = GlobalDfsInstance().AppendBufferToFile(uploadBuf, uploadBufLen,
                parentBlockId);
        if (ret)
        {
            LOG_ERROR("Failed to append \'%s\' to %s:%s, rc:%d:%s",
                    uploadBuf, parentName, parentBlockId, ret, strerror(ret));
            FreeMetaData(&parentMeta, true);
            DelMetaData(metaDataStore);
            return ret;
        }
    } else
    {
        ret = GlobalDfsInstance().AppendBuffer(uploadBuf, uploadBufLen,
                parentBlockId);
        if (ret)
        {
            LOG_ERROR("Failed to upload \'%s\' to %s:%s, rc:%d:%s",
                    uploadBuf, parentName, parentBlockId, ret, strerror(ret));
            FreeMetaData(&parentMeta, true);
            DelMetaData(metaDataStore);
            return ret;
        }

        //parentBlockId has been modified
        ret = SetMetaData(*parentMeta);
        if (ret)
        {
            LOG_ERROR(
                    "Failed to update parentMeta's blockid, parent path:%s:%s, blockid:%s, rc:%d:%s",
                    parentName, parentOID, parentBlockId, ret, strerror(ret));
            FreeMetaData(&parentMeta, true);
            DelMetaData(metaDataStore);
            return ret;
        }
    }

    FreeMetaData(&parentMeta);
    return STATUS_SUCCESS;
}

int CFuse::CreateHardLinkDG(MetaDataStore *fromMeta)
{
    int ret = STATUS_SUCCESS;

    DataGeography *dgMeta = NULL;
    int dgLen = sizeof(DataGeography) + sizeof(BlockGeography);
    bool noBlock = strlen(fromMeta->mMetaData.mBlockId) == 0 ? true : false;

    if (noBlock)
    {
        dgLen = sizeof(DataGeography);
    }

    ret = GlobalDGCache()->AllocKeyV(fromMeta->mMetaData.mObjId,
            (char **) &dgMeta, dgLen);
    if (ret)
    {
        LOG_ERROR(
                "Failed to alloc DataGeography meta of %s:%s, rc:%d:%s",
                fromMeta->mMetaData.mFileName, fromMeta->mMetaData.mObjId, ret, strerror(ret));
        return ret;
    }

    dgMeta->mVersion = VERSION_NUM;
    dgMeta->mHardLinkNum = 1;
    if (noBlock)
    {
        dgMeta->mBlockNum = 0;
    } else
    {

        dgMeta->mBlockNum = 1;

        BlockGeography &blockGeography = dgMeta->mBlocks[0];
        blockGeography.mOffset = 0;
        blockGeography.mLen = fromMeta->mMetaData.mStat.st_size;
        blockGeography.mStatus = 0;
        strcpy(blockGeography.mBlockId, fromMeta->mMetaData.mBlockId);
    }

    ret = GlobalDGCache()->SaveKeyV(fromMeta->mMetaData.mObjId, (char *) dgMeta,
            dgLen);
    if (ret)
    {
        LOG_ERROR(
                "Failed to Save DataGeography meta of %s:%s, rc:%d:%s",
                fromMeta->mMetaData.mFileName, fromMeta->mMetaData.mObjId, ret, strerror(ret));

        GlobalDGCache()->PutKeyV(fromMeta->mMetaData.mObjId, true);
        return ret;
    }

    ret = GlobalDGCache()->PutKeyV(fromMeta->mMetaData.mObjId);
    if (ret)
    {
        LOG_ERROR(
                "Failed to free DataGeography meta of %s:%s, rc:%d:%s",
                fromMeta->mMetaData.mFileName, fromMeta->mMetaData.mObjId, ret, strerror(ret));

        return ret;
    }

    std::string oldBlockId(fromMeta->mMetaData.mBlockId);
    memset(fromMeta->mMetaData.mMaxIdValue, 0,
            sizeof(fromMeta->mMetaData.mMaxIdValue));
    strcpy(fromMeta->mMetaData.mDGOID, fromMeta->mMetaData.mObjId);

    fromMeta->mMetaData.mExtension = ADFS_DATA_GEOG_EXTENSION;

    ret = SetMetaData(*fromMeta);
    if (ret)
    {
        LOG_ERROR(
                "Failed to update DataGeography meta of %s:%s, rc:%d:%s",
                fromMeta->mMetaData.mFileName, fromMeta->mMetaData.mObjId, ret, strerror(ret));

        fromMeta->mMetaData.mExtension = ADFS_DATA_GEOG_SIMPLE;
        strcpy(fromMeta->mMetaData.mBlockId, oldBlockId.c_str());

        GlobalDGCache()->DelKeyV(fromMeta->mMetaData.mObjId);

        return ret;
    }

    return STATUS_SUCCESS;
}

int CFuse::InsertHardLink(MetaDataStore &toMeta, const char *hardlinkOID)
{
    if (hardlinkOID == NULL || strlen(hardlinkOID) == 0)
    {
        return STATUS_SUCCESS;
    }

    int ret = STATUS_SUCCESS;

    MetaDataStore *fromMeta = NULL;
    ret = GetMetaDataByID(hardlinkOID, &fromMeta);
    if (ret)
    {
        LOG_ERROR("Failed to get source metadata %s, rc:%d:%s",
                hardlinkOID, ret, strerror(ret));
        return ret;
    }

    if (fromMeta->mMetaData.mExtension == ADFS_DATA_GEOG_SIMPLE)
    {
        ret = CreateHardLinkDG(fromMeta);
        if (ret)
        {
            LOG_ERROR(
                    "Failed to Create Data Geography meta %s:%s, rc:%d:%s",
                    fromMeta->mMetaData.mFileName, hardlinkOID, ret, strerror(ret));
            FreeMetaData(&fromMeta, true);
            return ret;
        }

    }

    DataGeography *dgMeta = NULL;
    int dgLen = 0;
    ret = GlobalDGCache()->GetKeyV(fromMeta->mMetaData.mDGOID,
            (char **) &dgMeta, &dgLen);
    if (ret)
    {
        LOG_ERROR(
                "Failed to get DataGeographyStore meta of %s:%s, rc:%d:%s",
                fromMeta->mMetaData.mFileName, fromMeta->mMetaData.mDGOID, ret, strerror(ret));

        FreeMetaData(&fromMeta, true);
        return ret;
    }

    dgMeta->mHardLinkNum++;

    ret = GlobalDGCache()->SaveKeyV(fromMeta->mMetaData.mDGOID, (char *) dgMeta,
            dgLen);
    if (ret)
    {
        LOG_ERROR(
                "Failed to Save DataGeography meta of %s:%s, rc:%d:%s",
                fromMeta->mMetaData.mFileName, fromMeta->mMetaData.mDGOID, ret, strerror(ret));

        GlobalDGCache()->PutKeyV(fromMeta->mMetaData.mDGOID, true);

        FreeMetaData(&fromMeta, true);
        return ret;
    }

    GlobalDGCache()->PutKeyV(fromMeta->mMetaData.mDGOID);

    strcpy(toMeta.mMetaData.mDGOID, fromMeta->mMetaData.mDGOID);
    toMeta.mMetaData.mExtension = ADFS_DATA_GEOG_EXTENSION;

    //@@@ Skip Save toMeta

    FreeMetaData(&fromMeta);

    return STATUS_SUCCESS;
}

int CFuse::InsertNodeMeta(MetaDataStore &metaDataStore, const char *parentOID,
        const char *hardlinkOID, const char *softLinkPath)
{
    if (softLinkPath && strlen(softLinkPath))
    {
        strcpy(metaDataStore.mMetaData.mAdfsNode.mSysLink.mSource,
                softLinkPath);
        /** @@@ Skip save metaData
         * Reason:
         * 1. skip save dht request
         * 2. easy to revert in next step
         */

    }

    int ret = InsertHardLink(metaDataStore, hardlinkOID);
    if (ret)
    {
        LOG_ERROR("Failed to insert node to hardlink list, own:%s, ret:%d:%s",
                metaDataStore.mMetaData.mObjId, ret, strerror(ret));

        //@@@ InsertHardLink don't save metaDataStore
        return ret;
    }

    ret = InsertTreeMeta(metaDataStore, parentOID);
    if (ret)
    {
        LOG_ERROR("Failed to insert node to tree, own %s, ret %d:%s",
                metaDataStore.mMetaData.mObjId, ret, strerror(ret));
        return ret;
    }

    return STATUS_SUCCESS;

}

int CFuse::RmTreeMeta(MetaDataStore &rmDirMeta)
{
    int ret = STATUS_SUCCESS;

    // Here first get the parent node, 
    // this is to avoid dead lock
    MetaDataStore *pParentMeta = NULL;
    char *pParentOID = rmDirMeta.mMetaData.mAdfsNode.mTreeNode.mParentId;
    ret = GetMetaDataByID(pParentOID, &pParentMeta);
    if (ret)
    {
        LOG_ERROR("Failed to get %s parent's meta data %s, rc:%d:%s",
                rmDirMeta.mMetaData.mFileName, pParentOID, ret, strerror(ret));
        return ret;
    }

    char *pParentName = pParentMeta->mMetaData.mFileName;
    char *pParentBlockid = pParentMeta->mMetaData.mBlockId;
    if (strlen(pParentBlockid) == 0)
    {
        LOG_ERROR("Parent no blockid, parent %s:%s", pParentName, pParentOID);
        FreeMetaData(&pParentMeta);
        return ENOENT;
    }

    struct flock parentLock;
    memset(&parentLock, 0, sizeof(parentLock));

    parentLock.l_type = F_WRLCK;
    parentLock.l_whence = SEEK_SET;
    parentLock.l_len = 0;

    //backup parent blockId
    char oldParentBlockID[BLOCKID_LEN] = { 0 };
    strncpy(oldParentBlockID, pParentBlockid, BLOCKID_LEN - 1);

    ret = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
    if (ret)
    {
        LOG_ERROR("Failed to lock parent content file, %s:%s, rc:%d:%s",
                pParentName, pParentBlockid, ret, strerror(ret));
        FreeMetaData(&pParentMeta);
        return ret;

    }

    char tempName[FILENAME_LENGTH_MAX] = { 0 };
    GenerateTempFile(pParentOID, tempName);

    s64_t fileSize;
    ret = GlobalDfsInstance().DownloadFile(tempName, pParentBlockid, &fileSize);
    if (ret)
    {
        LOG_ERROR("Failed to get parent content file, %s:%s, rc:%d:%s",
                pParentName, pParentBlockid, ret, strerror(ret));

        parentLock.l_type = F_UNLCK;

        int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);
        return ret;
    }

    std::string newTempFile(tempName);
    newTempFile += ".new.dir";

    FILE *oldFd = fopen(tempName, "r");
    if (oldFd == NULL)
    {
        LOG_ERROR("Failed to open parent's temp file %s:%s, error:%d:%s",
                pParentName, tempName, errno, strerror(errno));

        ::unlink(tempName);

        parentLock.l_type = F_UNLCK;

        int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);

        return ENOENT;
    }

    FILE *newFd = fopen(newTempFile.c_str(), "w+");
    if (newFd == NULL)
    {
        LOG_ERROR("Failed to open parent's temp file %s:%s, error:%d:%s",
                pParentName, newTempFile.c_str(), errno, strerror(errno));

        ::unlink(tempName);

        parentLock.l_type = F_UNLCK;

        int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);

        return ENOENT;
    }

    bool found = false;
    bool occurError = false;
    char *rmName = rmDirMeta.mMetaData.mFileName;
    u32_t dirEntryLen = sizeof(DirEntry) + 1;
    u32_t lineNum = 0;
    while (1)
    {
        char line[ONE_KILO];
        if (fread(line, 1, dirEntryLen, oldFd) < dirEntryLen)
        {
            break;
        }

        DirEntry dirEntry;
        memcpy(&dirEntry, line, sizeof(dirEntry));

        //worry strcmp isn't suitable for utf8
        if (strncmp(dirEntry.mName, rmName, FILENAME_LENGTH_MAX) == 0)
        {
            found = true;
            continue;
        }

        if (fwrite(line, 1, dirEntryLen, newFd) < dirEntryLen)
        {
            LOG_ERROR(
                    "Failed to write %s to parent's %s new tempfile %s, rc:%d:%s",
                    line, pParentName, newTempFile.c_str(), errno, strerror(errno));
            occurError = true;
            break;
        }
        lineNum++;
    }

    if (occurError == true)
    {
        LOG_ERROR("Occur error when rm %s in parent's content file %s",
                rmName, tempName);

        fclose(newFd);
        ::unlink(newTempFile.c_str());

        fclose(oldFd);
        ::unlink(tempName);

        parentLock.l_type = F_UNLCK;

        int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);
        return EAGAIN;
    }

    if (found == false)
    {
        LOG_WARN("Failed to found %s in parent's content file %s",
                rmName, tempName);

        fclose(newFd);
        ::unlink(newTempFile.c_str());

        fclose(oldFd);
        ::unlink(tempName);

        parentLock.l_type = F_UNLCK;

        int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);
        return STATUS_SUCCESS;
    }

    fclose(oldFd);
    ::unlink(tempName);

    fclose(newFd);

    char newBlockId[BLOCKID_LEN] = { 0 };
    ret = GlobalDfsInstance().AppendFile(newTempFile.c_str(), newBlockId);
    if (ret)
    {
        LOG_ERROR("Failed upload new content file %s for parent %s, rc:%d:%s",
                newTempFile.c_str(), pParentName, ret, strerror(ret));

        ::unlink(newTempFile.c_str());

        parentLock.l_type = F_UNLCK;

        int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);

        return ret;
    }
    ::unlink(newTempFile.c_str());

    strncpy(pParentMeta->mMetaData.mBlockId, newBlockId, BLOCKID_LEN - 1);
    ret = SetMetaData(*pParentMeta);
    if (ret)
    {
        LOG_ERROR("Failed to update parent %s blockid %s, rc:%d:%s",
                pParentName, newBlockId, ret, strerror(ret));

        int tempRc = GlobalDfsInstance().DeleteFile(newBlockId, fileSize);
        if (tempRc)
        {
            LOG_WARN("Failed to delete new directory content file:%s:%s",
                    pParentName, newBlockId, tempRc, strerror(tempRc));
        }

        parentLock.l_type = F_UNLCK;

        tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
        if (tempRc)
        {
            LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                    pParentName, pParentBlockid, tempRc, strerror(tempRc));

        }

        FreeMetaData(&pParentMeta);

        return ret;
    }

    parentLock.l_type = F_UNLCK;

    int tempRc = FileLock(oldParentBlockID, F_SETLKW, &parentLock);
    if (tempRc)
    {
        LOG_ERROR("Failed to unlock parent content file, %s:%s, rc:%d:%s",
                pParentName, pParentBlockid, tempRc, strerror(tempRc));

    }

    GlobalDfsInstance().DeleteFile(oldParentBlockID, fileSize);

    FreeMetaData(&pParentMeta);

    return STATUS_SUCCESS;
}

int CFuse::RmNodeMeta(MetaDataStore &rmDirMeta)
{
    int ret = RmTreeMeta(rmDirMeta);
    if (ret)
    {
        LOG_ERROR("Failed to rm tree metadata :%s", rmDirMeta.mMetaData.mObjId);
        return ret;
    }

    ret = DelMetaData(rmDirMeta);
    if (ret)
    {
        LOG_WARN(
                "Failed to delete %s:%s metadata, rc:%d:%s",
                rmDirMeta.mMetaData.mFileName, rmDirMeta.mMetaData.mObjId, ret, strerror(ret));
    }

    return STATUS_SUCCESS;
}

int CFuse::RmNodeMeta(const char *path)
{
    MetaDataStore *rmMeta = NULL;

    int ret = STATUS_SUCCESS;

    ret = GetMetaDataByPath(path, &rmMeta);
    if (ret)
    {
        LOG_ERROR("Failed to get %s metadata", path);
        return ret;
    }

    ret = RmNodeMeta(*rmMeta);
    if (ret)
    {
        LOG_ERROR("Failed to remove %s metada", path);

        FreeMetaData(&rmMeta, true);
        return ret;
    }

    FreeMetaData(&rmMeta);

    return STATUS_SUCCESS;
}

int CFuse::getdir(const char *path, fuse_dirh_t dh, fuse_dirfil_t fill)
{
    LOG_TRACE("Path:%s", path);
    // Don't need implement this interface, it is deprecated 
    LOG_WARN(
            "This function is deprecated, should use opendir/readir/releasedir");

    return 0;
}

int CFuse::fgetattr(const char *path, struct stat *stbuf,
        struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s", path);

    int ret = STATUS_SUCCESS;

#if 1
    return getattr(path, stbuf);
#else
    adfs_filehandler *fh = (adfs_filehandler *)(uintptr_t)fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler");
        return -ENOENT;
    }

    if (fh->fd)
    {
        ret = fstat(fh->fd, stbuf);
        if (ret == -1)
        {
            LOG_WARN("Failed to get temp :%s stat, path: %s, rc:%d:%s",
                    fh->tempName, path, errno, strerror(errno));
            //return -errno;
        }
        else
        {
            return STATUS_SUCCESS;
        }
    }

    MetaDataStore *queryMeta = NULL;
    ret = GetMetaDataByID(fh->objId, &queryMeta, META_READ);
    if (ret)
    {
        LOG_ERROR("No  %s:%s 's metaData, rc:%d:%s",
                path, fh->objId, ret, strerror(ret));
        return -ret;
    }

    *stbuf = queryMeta->mMetaData.mStat;

    FreeMetaData(&queryMeta, false, META_READ);
    return 0;
#endif
}

int CFuse::access(const char *path, int mask)
{
    LOG_TRACE("Path:%s, mask:0x%x", path, mask);
    //Using  default_permissions, so skip permission check 
    LOG_WARN("Not implement this function, use default permissions");
    return 0;
}

int CFuse::readlink(const char *path, char *buf, size_t size)
{
    LOG_TRACE("Path:%s, size:%lld", path, (long long)size);

    int ret = STATUS_SUCCESS;

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    MetaDataStore *linkMeta = NULL;
    ret = GetMetaDataByID(objId, &linkMeta, META_READ);
    if (ret)
    {
        LOG_ERROR("Failed to get link metadata %s, rc:%d:%s",
                objId, ret, strerror(ret));
        return -ret;
    }

    char *fileName = linkMeta->mMetaData.mAdfsNode.mSysLink.mSource;
    int fileNameLen = strlen(fileName);
    if (fileNameLen == 0)
    {
        LOG_ERROR("Failed to find target link name, path:%s", path);
        FreeMetaData(&linkMeta, false, META_READ);
        return -ENOENT;
    } else if (fileNameLen >= size)
    {
        LOG_ERROR(
                "No buffer to store the target link name, path:%s, size:%d, source:%s",
                path, (int)size, fileName);
        FreeMetaData(&linkMeta, false, META_READ);
        return -ENOMEM;
    }

    strcpy(buf, fileName);

    FreeMetaData(&linkMeta, false, META_READ);

    return 0;
}

int CFuse::mknod(const char *path, mode_t mode, dev_t rdev)
{
    LOG_TRACE("Path:%s, mode:0x%x, rdev:0x%x", path, (int)mode, (int)rdev);
    LOG_WARN("Don't support this function right now");

#if 0    
    int ret = STATUS_SUCCESS;
#endif
    return STATUS_SUCCESS;
}

int CFuse::RmData(MetaDataStore *pRmMeta)
{
    LOG_INFO("Successfully delete %s", pRmMeta->mMetaData.mFileName);

    if (strlen(pRmMeta->mMetaData.mMaxIdValue) == 0)
    {
        return STATUS_SUCCESS;
    }

    if (pRmMeta->mMetaData.mExtension == ADFS_DATA_GEOG_SIMPLE)
    {
        s64_t fsSize = 0;

        GlobalDfsInstance().DeleteFile(pRmMeta->mMetaData.mBlockId, fsSize);

        return STATUS_SUCCESS;
    }

    int ret = STATUS_SUCCESS;

    DataGeography *dgMeta = NULL;
    int dgLen = 0;
    ret = GlobalDGCache()->GetKeyV(pRmMeta->mMetaData.mDGOID, (char **) &dgMeta,
            &dgLen);
    if (ret)
    {
        LOG_WARN(
                "Failed to get %s:%s's DataGeographyStore meta, rc:%d:%s",
                pRmMeta->mMetaData.mFileName, pRmMeta->mMetaData.mDGOID, ret, strerror(ret));
        return ret;
    }

    dgMeta->mHardLinkNum--;
    if (dgMeta->mHardLinkNum == 0)
    {
        ClientDeleteEvent *pRmEvent = new ClientDeleteEvent(
                pRmMeta->mMetaData.mExtension, pRmMeta->mMetaData.mDGOID);
        char *pRmDG = NULL;
        if (HandleClientEvent(pRmEvent, &pRmDG))
        {
            LOG_WARN("Failed to delete old file:%s, file:%s, rc:%d:%s",
                    pRmMeta->mMetaData.mDGOID, ret, strerror(ret));
        }

        if (pRmDG)
        {
            delete pRmDG;
            pRmDG = NULL;
        }

        GlobalDGCache()->DelKeyV(pRmMeta->mMetaData.mDGOID);

        GlobalDGCache()->PutKeyV(pRmMeta->mMetaData.mDGOID);

        return STATUS_SUCCESS;
    } else
    {
        ret = GlobalDGCache()->SaveKeyV(pRmMeta->mMetaData.mDGOID,
                (char *) dgMeta, dgLen);
        if (ret)
        {
            LOG_ERROR(
                    "Failed to save %s:%s's DataGeography meta, rc:%d:%s",
                    pRmMeta->mMetaData.mFileName, pRmMeta->mMetaData.mDGOID, ret, strerror(ret));

            GlobalDGCache()->PutKeyV(pRmMeta->mMetaData.mDGOID);

            return ret;
        }

        GlobalDGCache()->PutKeyV(pRmMeta->mMetaData.mDGOID);
        return STATUS_SUCCESS;
    }

}

int CFuse::Rm(const char *path)
{
    int ret = STATUS_SUCCESS;

    //Get ObjId
    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    MetaDataStore *pRmMeta = NULL;
    ret = GetMetaDataByID(objId, &pRmMeta);
    if (ret)
    {
        LOG_ERROR("Failed to get %s metadata, rc:%d:%s",
                path, ret, strerror(ret));
        return -ret;
    }

    //remove meta data
    ret = RmNodeMeta(*pRmMeta);
    if (ret)
    {
        LOG_ERROR("Failed to remove meta data, ret:%d", ret);
        FreeMetaData(&pRmMeta, true);
        return -ret;
    }

    if (S_ISLNK(pRmMeta->mMetaData.mStat.st_mode))
    {
        LOG_INFO("Successfully delete one symlink %s", path);

        FreeMetaData(&pRmMeta);

        return STATUS_SUCCESS;
    } else if (S_ISREG(pRmMeta->mMetaData.mStat.st_mode))
    {

        RmData(pRmMeta);
        FreeMetaData(&pRmMeta);

        return STATUS_SUCCESS;
    } else
    {
        LOG_INFO("Successfully delete none-regular file, %s",
                pRmMeta->mMetaData.mFileName);

        FreeMetaData(&pRmMeta);

        return STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

int CFuse::unlink(const char *path)
{
    LOG_TRACE("Path:%s", path);

    return Rm(path);
}

int CFuse::LinkPrecheck(const char *from, const char *to,
        CLinkPrecheck &linkPrecheck)
{
    int ret = STATUS_SUCCESS;

    if (strcmp(from, to) == 0)
    {
        LOG_WARN("From path is same as to %s", from);
        return EEXIST;
    }

    if (strlen(from) >= FILENAME_LENGTH_MAX)
    {
        LOG_ERROR("from %s is too long to store", from);
        return EINVAL;
    }

    GetFileName(to, linkPrecheck.mToFileName);
    if (linkPrecheck.mToFileName.size() >= FILENAME_LENGTH_MAX)
    {
        LOG_ERROR("Don't support file name exceed %d, to:%s",
                FILENAME_LENGTH_MAX, to);
        return EINVAL;
    }

    GetDirName(to, linkPrecheck.mToParentPath);
    if (strcmp(to, linkPrecheck.mToParentPath.c_str()) == 0)
    {
        LOG_ERROR("Link target's path is same as his parent, %s", to);
        return EINVAL;
    }

    char toOID[OBJID_LEN] = { 0 };
    ConvertObjId(to, toOID);

    MetaDataStore *toMeta = NULL;
    ret = GetMetaDataByID(toOID, &toMeta, META_READ);
    if (ret == 0)
    {
        LOG_ERROR("To:%s exist ", to);

        FreeMetaData(&toMeta, false, META_READ);
        return EEXIST;
    } else if (ret != NO_METADATA_OBJ)
    {
        LOG_ERROR("Failed to get %s metadata, rc:%d:%s",
                to, ret, strerror(ret));
        return ret;
    }

    if (linkPrecheck.mType != SOFT_LINK_TYPE)
    {

        char fromOID[OBJID_LEN] = { 0 };
        ConvertObjId(from, fromOID);
        linkPrecheck.mFromOID = fromOID;

        ret = GetMetaDataByID(fromOID, &linkPrecheck.mFromeMeta, META_READ);
        if (ret)
        {
            LOG_ERROR("Failed to get from %s metaData, rc:%d:%s",
                    from, ret, strerror(ret));
            return ret;
        }
    }

    return STATUS_SUCCESS;

}

/*
 *  Hardlink's from is absolute path
 */
int CFuse::CreateHardLink(const char *from, const char *to,
        CLinkPrecheck &linkPrecheck)
{
    int ret = STATUS_SUCCESS;

    MetaDataStore *fromMeta = linkPrecheck.mFromeMeta;
    if (fromMeta->mMetaData.mStat.st_mode & S_IFDIR)
    {
        LOG_ERROR("Don't allow hardlink one directory :%s", from);
        FreeMetaData(&fromMeta);
        return EISDIR;
    }

    char toOID[OBJID_LEN] = { 0 };
    ConvertObjId(to, toOID);

    MetaDataStore *toMeta = NULL;
    ret = AllocMetaData(toOID, &toMeta);
    if (ret)
    {
        LOG_ERROR("No memory to copy metadata from:%s, to:%s", from, to);
        FreeMetaData(&fromMeta);
        return ENOMEM;
    }

    CopyMetaData(*toMeta, *fromMeta);

    FreeMetaData(&fromMeta, false, META_READ);

    MetaData &metaData = toMeta->mMetaData;
    strcpy(metaData.mObjId, toOID);
    strcpy(metaData.mFileName, linkPrecheck.mToFileName.c_str());

    char toParentOID[OBJID_LEN] = { 0 };
    ConvertObjId(linkPrecheck.mToParentPath.c_str(), toParentOID);

    ret = InsertNodeMeta(*toMeta, toParentOID, linkPrecheck.mFromOID.c_str(),
            NULL);
    if (ret)
    {
        LOG_ERROR("Failed to insert metadata ParentOID:%s, FromOID:%s",
                toParentOID, linkPrecheck.mFromOID.c_str());

        FreeMetaData(&toMeta, true);
        return ret;
    }

    FreeMetaData(&toMeta);

    LOG_INFO("Successfully hard link %s to %s", from, to);
    return STATUS_SUCCESS;
}

/*
 * softlink's from is relative path, not absolute path
 */
int CFuse::CreateSoftLink(const char *from, const char *to,
        CLinkPrecheck &linkPrecheck)
{
    int ret = STATUS_SUCCESS;

    char toOID[OBJID_LEN] = { 0 };
    ConvertObjId(to, toOID);

    MetaDataStore *toMeta = NULL;
    ret = AllocMetaData(toOID, &toMeta);
    if (ret)
    {
        LOG_ERROR("No memory to copy metadata from:%s, to:%s", from, to);
        FreeMetaData(&linkPrecheck.mFromeMeta, false, META_READ);
        return ENOMEM;
    }

    MetaData &metaData = toMeta->mMetaData;
    strcpy(metaData.mObjId, toOID);
    strcpy(metaData.mFileName, linkPrecheck.mToFileName.c_str());

    // set link's stat
    struct stat &linkStat = metaData.mStat;

    linkStat.st_size = strlen(from);
    linkStat.st_mode = S_IFLNK | FILE_OP_MODE;
    linkStat.st_blocks = 0;

    linkStat.st_dev = MAGIC_DEV;
    linkStat.st_nlink = 1;
    linkStat.st_uid = getuid();
    linkStat.st_gid = getgid();
    linkStat.st_blksize = BLOCK_SECTOR_SIZE;

    time_t now;
    time(&now);
    linkStat.st_atime = now;
    linkStat.st_mtime = now;
    linkStat.st_ctime = now;

    char toParentOID[OBJID_LEN] = { 0 };
    ConvertObjId(linkPrecheck.mToParentPath.c_str(), toParentOID);

    ret = InsertNodeMeta(*toMeta, toParentOID, NULL, from);
    if (ret)
    {
        LOG_ERROR("Failed to insert metadata ParentOID:%s, to :%s",
                toParentOID, to);
        FreeMetaData(&toMeta, true);
        return ret;
    }

    FreeMetaData(&toMeta);

    LOG_INFO("Successfully soft link %s to %s", from, to);
    return STATUS_SUCCESS;
}

int CFuse::symlink(const char *from, const char *to)
{
    LOG_TRACE("from:%s, to:%s", from, to);

    int ret = STATUS_SUCCESS;

    CLinkPrecheck linkPrecheck(SOFT_LINK_TYPE);
    ret = LinkPrecheck(from, to, linkPrecheck);
    if (ret)
    {
        LOG_ERROR("Precheck failed");
        return -ret;
    }

    ret = CreateSoftLink(from, to, linkPrecheck);
    if (ret)
    {
        LOG_ERROR("Failed to create symlink from %s to %s", from, to);
        return -ret;
    }

    return 0;
}

int CFuse::link(const char *from, const char *to)
{
    LOG_TRACE("from:%s, to:%s", from, to);

    int ret = STATUS_SUCCESS;

    CLinkPrecheck linkPrecheck(HARD_LINK_TYPE);
    ret = LinkPrecheck(from, to, linkPrecheck);
    if (ret)
    {
        LOG_ERROR("Precheck failed");
        return -ret;
    }

    ret = CreateHardLink(from, to, linkPrecheck);
    if (ret)
    {
        LOG_ERROR("Failed to create hardlink from %s to %s", from, to);
        return -ret;
    }

    return 0;
}

/**
 * if from is file, it is simlar to (link, unlink)
 *
 */
int CFuse::rename(const char *from, const char *to)
{
    LOG_TRACE("from:%s, to:%s", from, to);

    int ret = STATUS_SUCCESS;

    CLinkPrecheck linkPrecheck(RENAME_TYPE);

    ret = LinkPrecheck(from, to, linkPrecheck);
    if (ret)
    {
        LOG_ERROR("Failed to linkPrecheck");
        return -ret;
    }

    MetaDataStore *fromMeta = linkPrecheck.mFromeMeta;

    char toOID[OBJID_LEN] = { 0 };
    ConvertObjId(to, toOID);

    MetaDataStore *toMeta = NULL;
    ret = AllocMetaData(toOID, &toMeta);
    if (toMeta == NULL)
    {
        LOG_ERROR("Failed to alloc memory for to metadata, from:%s, to:%s",
                from, to);
        FreeMetaData(&fromMeta, false, META_READ);
        return -ENOMEM;
    }

    //set toMeta
    CopyMetaData(*toMeta, *fromMeta);

    strcpy(toMeta->mMetaData.mObjId, toOID);
    strcpy(toMeta->mMetaData.mFileName, linkPrecheck.mToFileName.c_str());

    FreeMetaData(&fromMeta, false, META_READ);

    char toParentOID[OBJID_LEN] = { 0 };
    ConvertObjId(linkPrecheck.mToParentPath.c_str(), toParentOID);
    ret = InsertNodeMeta(*toMeta, toParentOID, NULL, NULL);
    if (ret)
    {
        LOG_ERROR("Failed to insert metadata to tree, from:%s, to:%s",
                from, to);
        FreeMetaData(&toMeta, true);
        return -ret;
    }
    FreeMetaData(&toMeta);

    ret = RmNodeMeta(from);
    if (ret)
    {
        int tempRc = RmNodeMeta(to);
        if (tempRc)
        {
            LOG_ERROR("Failed to revert to %s node in rename", to);
        }
    }

    LOG_INFO("Successfully rename %s to %s", from, to);
    return 0;
}

int CFuse::chmod(const char *path, mode_t mode)
{
    LOG_TRACE("Path:%s, mode:0x%x", path, mode);

    int ret = STATUS_SUCCESS;

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    MetaDataStore *metaDataStore = NULL;
    ret = GetMetaDataByID(objId, &metaDataStore);
    if (ret)
    {
        LOG_ERROR("Failed to get %s metadata, rc:%d:%s",
                path, ret, strerror(ret));
        return -ret;
    }

    struct stat &statBuf = metaDataStore->mMetaData.mStat;
    //statBuf.st_mode = (statBuf.st_mode && (~(u64_t)(S_IRWXU|S_IRWXG|S_IRWXO)))| mode;
    statBuf.st_mode = mode;

    ret = SetMetaData(*metaDataStore);
    if (ret)
    {
        LOG_ERROR("Failed to update metatData for %s, mode:0x%x, rc:%d:%s",
                path, mode, ret, strerror(ret));
        FreeMetaData(&metaDataStore, true);
        return -ret;
    }

    FreeMetaData(&metaDataStore);

    LOG_INFO("Successfully chmod path:%s, mode:0x%x", path, mode);
    return 0;
}

int CFuse::chown(const char *path, uid_t uid, gid_t gid)
{
    LOG_TRACE("Path:%s, uid:%d, gid:%d", path, (int)uid, (int)gid);

    int ret = STATUS_SUCCESS;

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    MetaDataStore *metaDataStore = NULL;
    ret = GetMetaDataByID(objId, &metaDataStore);
    if (ret)
    {
        LOG_ERROR("Failed to get %s metadata, rc:%d:%s",
                path, ret, strerror(ret));
        return -ret;
    }

    struct stat &statBuf = metaDataStore->mMetaData.mStat;
    statBuf.st_uid = uid;
    statBuf.st_gid = gid;

    ret = SetMetaData(*metaDataStore);
    if (ret)
    {
        LOG_ERROR(
                "Failed to update metatData for %s, uid:0x%x, gid:0x%x rc:%d:%s",
                path, (int)uid, (int)gid, ret, strerror(ret));
        FreeMetaData(&metaDataStore, true);
        return -ret;
    }

    FreeMetaData(&metaDataStore);

    LOG_INFO("Successfully chown Path:%s, uid:%d, gid:%d",
            path, (int)uid, (int)gid);
    return 0;
}

int CFuse::truncate(const char *path, off_t size)
{
    LOG_TRACE("Path:%s, off_t:%lld", path, (long long)size);

    int ret = STATUS_SUCCESS;

    struct fuse_file_info * fi = (struct fuse_file_info *) malloc(
            sizeof(struct fuse_file_info));
    if (fi == NULL)
    {
        LOG_ERROR("No memory for fuse_file_info, path:%s, size:%lld",
                path, (s64_t)size);
        return -ENOMEM;
    }
    memset(fi, 0, sizeof(*fi));

    fi->flags = O_RDWR;
    ret = open(path, fi);
    if (ret)
    {
        LOG_ERROR("Failed to open :%s, ret:%d:%s", path, ret, strerror(ret));
        free(fi);
        return ret;
    }

    ret = ftruncate(path, size, fi);
    if (ret)
    {
        LOG_ERROR("Failed to truncate:%s, ret:%d:%s", path, ret, strerror(ret));
        release(path, fi);
        free(fi);
        return ret;
    }

    ret = release(path, fi);
    if (ret)
    {
        LOG_ERROR("Failed to close %s, ret:%d:%s", path, ret, strerror(ret));
        free(fi);
        return ret;
    }

    free(fi);

    LOG_INFO("Successfully truncate Path:%s, off_t:%lld",
            path, (long long)size);
    return 0;
}

int CFuse::ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, size:%lld", path, (long long)size);

    int res;

    adfs_filehandler *fh = (adfs_filehandler *) (uintptr_t) fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler, path:%s, size:%lld", path, (s64_t)size);
        return -ENOENT;
    }

    res = ::ftruncate(fh->fd, size);
    ;
    if (res == -1)
    {
        LOG_ERROR("Failed to write data from %s, path:%s, rc:%d:%s",
                fh->tempName, path, errno, strerror(errno));
        return -errno;
    }

    fh->flag_dirty = 1;
    fh->flag_need_upload = 1;

    if (mWriteThrough)
    {
        fsync(path, 0, fi);
    }

    LOG_INFO("Successfully ftruncate Path:%s, size:%lld",
            path, (long long)size);
    return 0;
}

//Deprecated
int CFuse::utime(const char *path, struct utimbuf *ub)
{
    LOG_TRACE("Path:%s", path);
    return 0;
}

int CFuse::utimens(const char *path, const struct timespec ts[2])
{
    LOG_TRACE("Path:%s", path);

    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    MetaDataStore *metaData = NULL;
    int res = GetMetaDataByPath(path, &metaData);
    if (res)
    {
        LOG_ERROR("Failed to get metat data, path:%s, rc:%d:%s",
                path, res, strerror(res));
        return -res;
    }
    struct stat &mStat = metaData->mMetaData.mStat;

    mStat.st_atime = ts[0].tv_sec;
    mStat.st_mtime = ts[1].tv_sec;

    res = SetMetaData(*metaData);
    if (res)
    {
        LOG_ERROR(
                "Failed to update time of %s:%s, res:%d:%s",
                metaData->mMetaData.mObjId, metaData->mMetaData.mFileName, res, strerror(res));
        FreeMetaData(&metaData, true);
        return -res;
    }

    FreeMetaData(&metaData);

    LOG_INFO("Successfully utimens Path:%s", path);
    return 0;
}

int CFuse::InitFsMetrics(char **dhtValue, int *dhtValueLen)
{
    u64_t freeCapcity = GlobalDfsInstance().GetFreeCapacity();
    if (freeCapcity == 0)
    {
        LOG_ERROR("Failed to get free capacity");
        return ENOENT;
    }

    struct statvfs fsStat;
    fsStat.f_bsize = BLOCK_SECTOR_SIZE;
    fsStat.f_frsize = 4 * ONE_KILO;
    fsStat.f_blocks = (freeCapcity / BLOCK_SECTOR_SIZE);
    fsStat.f_bfree = fsStat.f_blocks;
    fsStat.f_bavail = fsStat.f_blocks;
    fsStat.f_files = 0;
    fsStat.f_ffree = 1 * ONE_GIGA;
    fsStat.f_favail = fsStat.f_ffree;
    fsStat.f_fsid = MAGIC_DEV;
    fsStat.f_flag = ST_NOATIME | ST_NODIRATIME;
    fsStat.f_namemax = FILENAME_LENGTH_MAX - 1;

    FsMetricStore *pFsMetric = (FsMetricStore *) malloc(sizeof(FsMetricStore));
    if (pFsMetric == NULL)
    {
        LOG_ERROR("Failed to alloc memory for FsMetricStore");
        return ENOMEM;
    }
    memset(pFsMetric, 0, sizeof(FsMetricStore));

    pFsMetric->mVersion = VERSION_NUM;
    pFsMetric->mFsMetric.mFsStat = fsStat;

    *dhtValueLen = sizeof(FsMetricStore);
    *dhtValue = (char *) pFsMetric;

    return STATUS_SUCCESS;

}

int CFuse::statfs(const char *path, struct statvfs *stbuf)
{
    LOG_TRACE("Path:%s", path);

    const std::string &nameSpace = GlobalMetaCache()->GetDHTNameSpace();
    const std::string &dhtKey = GlobalMetaCache()->GetDHTKey();

    char *dhtValue = NULL;
    int dhtValueLen = 0;
    int res = GlobalDhtInstance().GetKeyV(nameSpace.c_str(), DHT_FSMETRICS_OID,
            dhtKey.c_str(), &dhtValue, &dhtValueLen);
    if (res != 0 && res != ENOENT)
    {
        LOG_INFO(
                "Failed to get fsmetrics: namespace:%s, objId:%s, dhtKey:%s, res:%d:%s",
                nameSpace.c_str(), DHT_FSMETRICS_OID, dhtKey.c_str(), res, strerror(res));
        return -res;
    }
    if (res == ENOENT)
    {
        res = InitFsMetrics(&dhtValue, &dhtValueLen);
        if (res)
        {
            LOG_ERROR("Failed to InitFsMetrics");
            return -res;
        }
        //continue do the left thing
    }

    // the left is res == 0
    FsMetricStore *pFsMetric = (FsMetricStore *) dhtValue;
    struct statvfs &fsStat = pFsMetric->mFsMetric.mFsStat;

    s64_t sizeChange = 0;
    s64_t nodeChange = 0;

    GlobalDfsInstance().GetMetricsChange(nodeChange, sizeChange);

    s64_t costFsBlocks = (sizeChange + fsStat.f_frsize - 1) / fsStat.f_frsize;
    fsStat.f_bfree -= costFsBlocks;
    fsStat.f_bavail -= costFsBlocks;

    fsStat.f_files += nodeChange;
    fsStat.f_ffree -= nodeChange;
    fsStat.f_favail -= nodeChange;

    *stbuf = fsStat;

    res = GlobalDhtInstance().SaveKeyV(nameSpace.c_str(), DHT_FSMETRICS_OID,
            dhtKey.c_str(), dhtValue, dhtValueLen);

    if (res)
    {
        LOG_WARN("Failed to update fsmetrics to DHT");
    }

    free(dhtValue);

    LOG_INFO("Successfully statfs Path:%s", path);

    return 0;
}

/* xattr operations are optional and can safely be left unimplemented */
int CFuse::setxattr(const char *path, const char *name, const char *value,
        size_t size, int flags)
{
    LOG_TRACE("path:%s, name:%s, value:%s, size:%lld, flags:%d",
            path, name, value, (long long)size, flags);
#if 0
    int res = lsetxattr(path, name, value, size, flags);
    if (res == -1)
    return -errno;
#endif
    return 0;
}

int CFuse::getxattr(const char *path, const char *name, char *value,
        size_t size)
{
    LOG_TRACE("Path:%s, name:%s, size:%lld", path, name, (long long)size);
#if 0
    int res = lgetxattr(path, name, value, size);
    if (res == -1)
    return -errno;
#endif
    return 0;
}

int CFuse::listxattr(const char *path, char *list, size_t size)
{
    LOG_TRACE("Path:%s, list:%s, size:%lld", path, list, (long long)size);
#if 0
    int res = llistxattr(path, list, size);
    if (res == -1)
    return -errno;
#endif
    return 0;
}

int CFuse::removexattr(const char *path, const char *name)
{
    LOG_TRACE("Path:%s, name:%s", path, name);
#if 0
    int res = lremovexattr(path, name);
    if (res == -1)
    return -errno;
#endif
    return 0;
}

int CFuse::FileLock(const char *blockId, int cmd, struct flock *lock)
{
    return 0;
}

int CFuse::lock(const char *path, struct fuse_file_info *fi, int cmd,
        struct flock *lock)
{
    LOG_TRACE("Path:%s, cmd:%d", path, cmd);
#if 0
    (void) path;

    ret = ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
            sizeof(fi->lock_owner));
#endif
    return 0;
}

int CFuse::getattr(const char *path, struct stat *stbuf)
{
    LOG_TRACE("path:%s", path);

    MetaDataStore *metaData = NULL;
    int res = GetMetaDataByPath(path, &metaData, META_READ);
    if (res)
    {
        LOG_INFO("No metat data, path:%s, rc:%d:%s", path, res, strerror(res));
        //here use -res
        // due to system use 
        return -res;
    }
    *stbuf = metaData->mMetaData.mStat;

    FreeMetaData(&metaData, false, META_READ);

    return 0;
}

int CFuse::opendir(const char *path, struct fuse_file_info *fi)
{

    LOG_TRACE("Path:%s", path);
    struct adfs_dirp *d = (struct adfs_dirp *) malloc(sizeof(struct adfs_dirp));
    if (d == NULL)
    {
        LOG_ERROR("Failed to alloc memory for adfs_dirp, path:%s", path);
        return -ENOMEM;
    }
    memset(d, 0, sizeof(*d));

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);
    int res = GetMetaDataByID(objId, &d->metaData, META_READ);
    if (res)
    {
        LOG_ERROR("Failed to get metadata :%s, rc:%d:%s",
                path, res, strerror(res));
        if (d)
        {
            delete d;
        }
        return -res;
    }
    //0 means '.'
    //1 means '..'
    d->offset = 2;

    char *blockId = d->metaData->mMetaData.mBlockId;
    if (strlen(blockId) == 0)
    {
        d->dirContentFd = 0;
    } else
    {
        d->dirContentLock.l_type = F_RDLCK;
        d->dirContentLock.l_whence = SEEK_SET;
        d->dirContentLock.l_len = 0;

        res = FileLock(blockId, F_SETLKW, &d->dirContentLock);
        if (res)
        {
            LOG_ERROR("Failed to lock %s:%s, rc:%d:%s",
                    path, blockId, res, strerror(res));

            releasedir(d);
            return -res;
        }
        d->isLocked = true;

        char tempName[FILENAME_LENGTH_MAX] = { 0 };

        GenerateTempFile(objId, tempName);

        res = ::access(tempName, R_OK);
        if (res)
        {
            s64_t fileSize = 0;
            res = GlobalDfsInstance().DownloadFile(tempName, blockId,
                    &fileSize);
            if (res)
            {

                LOG_ERROR(
                        "Failed to download %s 's content file %s to local %s, rc:%d:%s",
                        path, blockId, tempName, res, strerror(res));

                releasedir(d);
                return -res;
            }
        }
        strcpy(d->dirContentName, tempName);

        d->dirContentFd = fopen(tempName, "r");
        if (d->dirContentFd == NULL)
        {
            LOG_ERROR("Failed to open %s 's content file  %s, rc:%d:%s",
                    path, tempName, errno, strerror(errno));

            releasedir(d);
            return -errno;
        }
    }

    fi->fh = (unsigned long) d;
    return 0;
}

int CFuse::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, offset:%lld", path, (long long)offset);

    u32_t addEntryNum = 0;

    struct adfs_dirp *d = (struct adfs_dirp *) (uintptr_t) fi->fh;

    int ret = STATUS_SUCCESS;

    if (offset == 0)
    {
        //fill "."
        if (filler(buf, ".", &d->metaData->mMetaData.mStat, ++offset))
        {
            LOG_WARN("No memory to store . entry under %s", path);
            return -ENOMEM;
        }

        addEntryNum++;
    }

    if (offset == 1)
    {
        MetaDataStore *parentMeta = NULL;

        char *parentId = d->metaData->mMetaData.mAdfsNode.mTreeNode.mParentId;

        ret = GetMetaDataByID(parentId, &parentMeta, META_READ);
        if (ret)
        {
            LOG_ERROR("Failed to get %s parent's %s metaData, rc:%d:%s",
                    path, parentId, ret, strerror(ret));
            return -ret;
        }

        if (filler(buf, "..", &parentMeta->mMetaData.mStat, ++offset))
        {
            LOG_ERROR("No memory to store .. entry under %s", path);
            FreeMetaData(&parentMeta, false, META_READ);
            return -ENOMEM;
        }

        FreeMetaData(&parentMeta, false, META_READ);

        addEntryNum++;
    }

    if (d->dirContentFd == NULL)
    {
        LOG_DEBUG("NO subentry under %s", path);
        return 0;
    }

    if (offset != d->offset)
    {
        ret = fseek(d->dirContentFd, 0, SEEK_SET);
        if (ret < 0)
        {
            LOG_ERROR("Failed to fseek to file begin, path:%s:%s rc:%d:%s",
                    path, d->dirContentName, errno, strerror(errno));
            return -errno;
        }

        u32_t readLines = 0;
        while (offset - 2 != readLines)
        {
            char line[ONE_KILO];

            char *data = fgets(line, sizeof(line), d->dirContentFd);
            if (data == NULL)
            {
                LOG_ERROR(
                        "Read %s:%s:%u lines occur error, rc:%d:%s",
                        path, d->dirContentName, readLines, errno, strerror(errno));
                return -errno;
            }

            if (strlen(line) == 0)
            {
                LOG_WARN("No entry in %s:%s:%u lines",
                        path, d->dirContentName, readLines);
                continue;
            }

            readLines++;
        }
    }

    ret = 0;
    while (1)
    {

        char line[ONE_KILO];
        char *data = fgets(line, sizeof(line), d->dirContentFd);
        if (data == NULL)
        {
            break;
        }

        if (strlen(line) == 0)
        {
            continue;
        }

        DirEntry dirEntry;
        memcpy(&dirEntry, line, sizeof(dirEntry));

        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = dirEntry.mInode;
        st.st_mode = dirEntry.mMode;

        if (filler(buf, dirEntry.mName, &st, d->offset + 1))
        {
            if (fseek(d->dirContentFd, -(sizeof(dirEntry) + 1), SEEK_CUR) < 0)
            {
                LOG_WARN("Failed to back one line, reset d->offset");
                d->offset = 0;
            }

            if (addEntryNum)
            {
                LOG_INFO("Fill %u entry in one loop, under %s, sub path:%s",
                        addEntryNum, path, dirEntry.mName);
                return 0;
            } else
            {
                LOG_WARN("Fill %u entry in one loop, under %s, sub path:%s",
                        addEntryNum, path, dirEntry.mName);
                return -ENOMEM;
            }
        }

        //@@@ TESTING CODE
        //LOG_INFO("Parent :%s, current node %s, the adding index:%u", 
        //    path, entryMetaData->mMetaData.mFileName, addEntryNum);
        d->offset++;

        addEntryNum++;
    }

    LOG_DEBUG("Fill %u entry in one loop, under %s", addEntryNum, path);
    return 0;
}

int CFuse::releasedir(struct adfs_dirp *d)
{
    int ret = 0;

    if (d->isLocked)
    {
        d->dirContentLock.l_type = F_UNLCK;

        ret = FileLock(d->metaData->mMetaData.mBlockId, F_SETLKW,
                &d->dirContentLock);
        if (ret)
        {
            LOG_ERROR(
                    "Failed to unlock dir %s 's content file %s, rc:%d:%s",
                    d->metaData->mMetaData.mFileName, d->metaData->mMetaData.mBlockId, ret, strerror(ret));
        }
    }

    FreeMetaData(&d->metaData, false, META_READ);

    if (d->dirContentFd)
    {
        fclose(d->dirContentFd);
    }

    if (strlen(d->dirContentName))
    {
        ::unlink(d->dirContentName);
    }

    free(d);

    return 0;
}

int CFuse::releasedir(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("%s", path);

    int ret = STATUS_SUCCESS;

    struct adfs_dirp *d = (struct adfs_dirp *) (uintptr_t) fi->fh;

    releasedir(d);

    return 0;
}

int CFuse::fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, datasync:%s", path, datasync);
    return 0;
}

int CFuse::mkdir(const char *path, mode_t mode)
{
    LOG_TRACE("Path:%s, mode:0x%x", path, mode);

    int ret = 0;

    std::string parent;
    GetDirName(path, parent);
    if (strcmp(path, parent.c_str()) == 0)
    {
        LOG_WARN("parent path is same as himself, path:%s", path);
        return -EEXIST;
    }

    std::string fileName;
    GetFileName(path, fileName);
    if (fileName.size() >= FILENAME_LENGTH_MAX)
    {
        LOG_ERROR("Don't support file name exceed %d, to:%s",
                FILENAME_LENGTH_MAX, path);
        return -EINVAL;
    }

    MetaDataStore *pMetaDataStore = NULL;
    ret = GetMetaDataByPath(path, &pMetaDataStore, META_READ);
    if (ret == STATUS_SUCCESS)
    {
        LOG_WARN("path:%s exit", path);
        FreeMetaData(&pMetaDataStore, false, META_READ);
        return -EEXIST;
    } else if (ret != NO_METADATA_OBJ)
    {
        LOG_ERROR("Failed to get metadata:%s, rc:%d:%s",
                path, ret, strerror(ret));
        return -ret;
    }

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    MetaDataStore *metaDataStore = NULL;
    ret = AllocMetaData(objId, &metaDataStore);
    if (ret)
    {
        LOG_ERROR("Failed to alloc memory for MetaDataStore of %s", path);
        return -ENOMEM;
    }

    InitMetaData(*metaDataStore, mode | S_IFDIR, path);

    char parentOID[OBJID_LEN] = { 0 };
    ConvertObjId(parent.c_str(), parentOID);
    ret = InsertNodeMeta(*metaDataStore, parentOID, NULL, NULL);
    if (ret)
    {
        LOG_ERROR("Failed to insert one metadata, rc:%d:%s",
                ret, strerror(ret));
        FreeMetaData(&metaDataStore, true);
        return -ret;
    }

    FreeMetaData(&metaDataStore);

    LOG_INFO("Successfully mkdir Path:%s, mode:0x%x", path, mode);

    return STATUS_SUCCESS;
}

int CFuse::rmdir(const char *path)
{
    LOG_TRACE("Path:%s", path);

    return Rm(path);
}

int CFuse::create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    LOG_TRACE("%s, mode:0x%x, flag:0x%x", path, (int)mode, fi->flags);

    int ret = 0;

    std::string parent;
    GetDirName(path, parent);
    if (strcmp(path, parent.c_str()) == 0)
    {
        LOG_ERROR("parent path is same as himself, path:%s", path);
        return -EEXIST;
    }

    std::string fileName;
    GetFileName(path, fileName);
    if (fileName.size() >= FILENAME_LENGTH_MAX)
    {
        LOG_ERROR("Don't support file name exceed %d, to:%s",
                FILENAME_LENGTH_MAX, path);
        return -EINVAL;
    }

    MetaDataStore *pMetaDataStore = NULL;
    ret = GetMetaDataByPath(path, &pMetaDataStore, META_READ);
    if (ret == STATUS_SUCCESS)
    {
        LOG_ERROR("path:%s exit", path);
        FreeMetaData(&pMetaDataStore, false, META_READ);
        return -EEXIST;
    } else if (ret != NO_METADATA_OBJ)
    {
        LOG_ERROR("Failed to get metadata:%s, rc:%d:%s",
                path, ret, strerror(ret));
        return -ret;
    }

    char parentOID[OBJID_LEN] = { 0 };
    ConvertObjId(parent.c_str(), parentOID);

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(path, objId);

    MetaDataStore *newFileMeta = NULL;
    ret = AllocMetaData(objId, &newFileMeta);
    if (ret)
    {
        LOG_ERROR("Failed to alloc memory for MetaDataStore of %s", path);
        return -ENOMEM;
    }

    InitMetaData(*newFileMeta, mode | S_IRUSR | S_IWUSR, path);

    ret = InsertNodeMeta(*newFileMeta, parentOID, NULL, NULL);
    if (ret)
    {
        LOG_ERROR("Failed to insert meta data %s", path);
        return -ret;
    }

    ret = open(path, fi);
    if (ret)
    {
        LOG_ERROR("Failed to open %s", path);
        RmNodeMeta(*newFileMeta);
        LOG_ERROR("Rm %s file", path);
        FreeMetaData(&newFileMeta, true);
        return ret;
    }

    FreeMetaData(&newFileMeta);

    LOG_INFO("Successfully create %s, mode:0x%x, flag:0x%x",
            path, (int)mode, fi->flags);
    return 0;
}

int CFuse::open(const char *path, struct fuse_file_info *fi)
{

    LOG_TRACE("Path:%s, flags:0x%x", path, fi->flags);

    int ret = STATUS_SUCCESS;

    adfs_filehandler *fh = (adfs_filehandler *) malloc(
            sizeof(adfs_filehandler));
    if (fh == NULL)
    {
        LOG_ERROR("Failed to alloc memory for %s", path);
        return -ENOMEM;
    }
    memset(fh, 0, sizeof(*fh));

    ConvertObjId(path, fh->objId);
    MetaDataStore *openFileMeta = NULL;
    int res = GetMetaDataByID(fh->objId, &openFileMeta, META_READ);
    if (res)
    {
        LOG_ERROR("Failed to get metadata:%s:%s, rc:%d:%s",
                path, fh->objId, res, strerror(res));
        return -res;
    }

    fh->path = strdup(path);
    if (fh->path == NULL)
    {
        LOG_ERROR("Failed to alloc memory to store path %s", path);
        FreeMetaData(&openFileMeta, false, META_READ);
        releaseFh(fh);
        return -ENOMEM;
    }

    std::string parentName;
    GetDirName(path, parentName);
    ConvertObjId(parentName.c_str(), fh->parentOID);

    GenerateTempFile(fh->objId, fh->tempName);

    struct stat statBuf;
    ret = stat(fh->tempName, &statBuf);
    if (ret == 0)
    {
        time_t now_time;
        time(&now_time);
        char timeStr[FILENAME_LENGTH_MAX] = { 0 };
        snprintf(timeStr, FILENAME_LENGTH_MAX - 1, ".%lld", (s64_t) now_time);
        strcat(fh->tempName, timeStr);
        LOG_DEBUG("open %s more times, new temp file %s", path, fh->tempName);
    }

    ClientDownloadEvent *event = new ClientDownloadEvent(fh->tempName,
            openFileMeta->mMetaData.mExtension,
            openFileMeta->mMetaData.mMaxIdValue);
    if (event == NULL)
    {
        LOG_ERROR("Failed to alloc ClientDownloadEvent %s:%s",
                path, openFileMeta->mMetaData.mMaxIdValue);
        FreeMetaData(&openFileMeta, false, META_READ);
        releaseFh(fh);
        return -ENOMEM;
    }

    ret = HandleClientEvent(event, (char **) &fh->dg);
    if (ret)
    {
        LOG_ERROR("Failed to handle ClientDownloadEvent %s:%s",
                path, openFileMeta->mMetaData.mMaxIdValue);
        FreeMetaData(&openFileMeta, false, META_READ);
        releaseFh(fh);
        return -ret;
    }

    memset(&statBuf, 0, sizeof(statBuf));
    ret = stat(fh->tempName, &statBuf);
    if (ret == 0)
    {
        fh->fd = ::open(fh->tempName, fi->flags);
    } else
    {
        //The file has no data
        fh->fd = ::open(fh->tempName, fi->flags | O_CREAT,
                openFileMeta->mMetaData.mStat.st_mode);
    }

    if (fh->fd == -1)
    {
        LOG_ERROR(
                "Failed to open %s, path:%s, flags:0x%x, mode:0x%x, rc:%d:%s",
                fh->tempName, path, fi->flags, openFileMeta->mMetaData.mStat.st_mode, errno, strerror(errno));
        FreeMetaData(&openFileMeta, false, META_READ);
        releaseFh(fh);
        return -errno;
    }

    fi->fh = (unsigned long) fh;
    FreeMetaData(&openFileMeta, false, META_READ);

    LOG_DEBUG("Successfully open Path:%s, flags:0x%x", path, fi->flags);
    return 0;
}

int CFuse::read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    LOG_TRACE("%s, size:%lld, offset:%lld", path, (s64_t)size, (s64_t)offset);
    int res;

    adfs_filehandler *fh = (adfs_filehandler *) (uintptr_t) fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler, path:%s", path);
        return -ENOENT;
    }

    res = pread(fh->fd, buf, size, offset);
    if (res == -1)
    {
        LOG_ERROR("Failed to read data from %s, path:%s, rc:%d:%s",
                fh->tempName, path, errno, strerror(errno));
        return -errno;
    }

    return res;
}

int CFuse::write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    LOG_TRACE("path:%s, size:%lld, offset:%lld",
            path, (s64_t)size, (s64_t)offset);
    int res;

    adfs_filehandler *fh = (adfs_filehandler *) (uintptr_t) fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler path:%s", path);
        return -ENOENT;
    }

    res = pwrite(fh->fd, buf, size, offset);
    if (res == -1)
    {
        LOG_ERROR("Failed to write data from %s, rc:%d:%s",
                fh->tempName, errno, strerror(errno));
        return -errno;
    }

    fh->flag_dirty = 1;
    fh->flag_need_upload = 1;
    return res;
}

int CFuse::flush(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("path:%s", path);
    int res;

    adfs_filehandler *fh = (adfs_filehandler *) (uintptr_t) fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler, path:%s", path);
        return -ENOENT;
    }

    //temp file is the cache, so just fsync to local file
    if (fh->flag_dirty)
    {
        res = ::fsync(fh->fd);
        if (res == -1)
        {
            LOG_ERROR("Failed to fsync:%s, path %s, rc:%d:%s",
                    fh->tempName, path, errno, strerror(errno));
            return -errno;
        }
        fh->flag_dirty = 0;
    }

    //according to spec, flush shouldn't make sure flush data to server
    //but a lot of tools use flush as flush data to server/disk method
    // so add this code here
#if !UPLOAD_ON_CLOSE
    //sync data to server
    res = upload(fh);
#endif

    return res;
}

int CFuse::SetDG(MetaDataStore &uploadFileMeta, DataGeography *pDG)
{
    if (pDG == NULL)
    {
        LOG_ERROR("Something is wrong, pDG is NULL");
        return ENOENT;
    }

    int ret = STATUS_SUCCESS;

    u32_t oldExtension = uploadFileMeta.mMetaData.mExtension;
    char oldId[BLOCKID_LEN] = { 0 };
    strcpy(oldId, uploadFileMeta.mMetaData.mMaxIdValue);

    bool replace = false;
    u32_t hardLinkNum = 1;

    if (strlen(oldId))
    {
        ClientDeleteEvent *pRmEvent = new ClientDeleteEvent(oldExtension,
                oldId);
        char *pRmDG = NULL;
        if (HandleClientEvent(pRmEvent, &pRmDG))
        {
            LOG_WARN("Failed to delete old file:%s, rc:%d:%s",
                    oldId, ret, strerror(ret));
        }
        if (pRmDG)
        {
            delete pRmDG;
            pRmDG = NULL;
        }

        if (oldExtension == ADFS_DATA_GEOG_SIMPLE)
        {
            //Don't need to do anything
        } else
        {
            DataGeography *pOldDG = NULL;
            int oldDGLen = 0;
            ret = GlobalDGCache()->GetKeyV(oldId, (char **) &pOldDG, &oldDGLen);
            if (ret)
            {
                LOG_ERROR("Failed to get oldDG %s, rc:%d:%s",
                        oldId, ret, strerror(ret));
                return ret;
            }

            if (pOldDG->mHardLinkNum > 1)
            {
                //The data exist hard link
                replace = true;
                hardLinkNum = pOldDG->mHardLinkNum;

            }

            GlobalDGCache()->DelKeyV(oldId);
            GlobalDGCache()->PutKeyV(oldId);

        }

    }

    if (pDG->mBlockNum == 0 && replace == false)
    {
        uploadFileMeta.mMetaData.mStat.st_size = 0;
        uploadFileMeta.mMetaData.mStat.st_blocks = 0;

        uploadFileMeta.mMetaData.mExtension = ADFS_DATA_GEOG_SIMPLE;
        memset(uploadFileMeta.mMetaData.mMaxIdValue, 0,
                sizeof(uploadFileMeta.mMetaData.mMaxIdValue));

        return STATUS_SUCCESS;
    } else if (pDG->mBlockNum == 1 && replace == false)
    {
        uploadFileMeta.mMetaData.mExtension = ADFS_DATA_GEOG_SIMPLE;
        strcpy(uploadFileMeta.mMetaData.mBlockId, pDG->mBlocks[0].mBlockId);

        uploadFileMeta.mMetaData.mStat.st_size = pDG->mBlocks[0].mLen;
        uploadFileMeta.mMetaData.mStat.st_blocks = pDG->mBlocks[0].mLen
                / BLOCK_SECTOR_SIZE;

        return STATUS_SUCCESS;
    }

    char newDGID[OBJID_LEN] = { 0 };
    strcpy(newDGID, uploadFileMeta.mMetaData.mObjId);
    if (replace)
    {
        strcpy(newDGID, oldId);
    }

    DataGeography *pSaveDG = NULL;
    int dgLen = sizeof(DataGeography) + pDG->mBlockNum * sizeof(BlockGeography);

    ret = GlobalDGCache()->AllocKeyV(newDGID, (char **) &pSaveDG, dgLen);
    if (ret)
    {
        LOG_ERROR("Failed to alloc DataGeography %s:%d, rc:%d:%s",
                newDGID, dgLen, ret, strerror(ret));
        return ENOMEM;
    }

    memcpy(pSaveDG, pDG, dgLen);
    pSaveDG->mHardLinkNum = hardLinkNum;

    ret = GlobalDGCache()->SaveKeyV(newDGID, (char *) pSaveDG, dgLen);
    if (ret)
    {
        LOG_ERROR("Failed to save DataGeography %s:%d, rc:%d:%s",
                newDGID, dgLen, ret, strerror(ret));

        GlobalDGCache()->PutKeyV(newDGID, true);
        return ret;
    }

    GlobalDGCache()->PutKeyV(newDGID);

    u64_t fileSize = 0;
    for (int i = 0; i < pDG->mBlockNum; i++)
    {
        fileSize += pDG->mBlocks[i].mLen;
    }

    uploadFileMeta.mMetaData.mStat.st_size = fileSize;
    uploadFileMeta.mMetaData.mStat.st_blocks = fileSize / BLOCK_SECTOR_SIZE;

    uploadFileMeta.mMetaData.mExtension = ADFS_DATA_GEOG_EXTENSION;
    strcpy(uploadFileMeta.mMetaData.mDGOID, newDGID);

    return STATUS_SUCCESS;
}

int CFuse::upload(adfs_filehandler *fh)
{
    if (fh->flag_need_upload == 0)
    {
        return 0;
    }

    MetaDataStore *uploadFileMeta = NULL;
    int ret = GetMetaDataByID(fh->objId, &uploadFileMeta);
    if (ret)
    {
        //error
        LOG_ERROR("Failed to get upload file's metadata:%s:%s, rc:%d:%s",
                fh->objId, fh->path, ret, strerror(ret));
        return -ret;
    }

    LOG_DEBUG("Begin to upload one old file:%s", fh->path);

    ClientUploadEvent *pUploadEvent = new ClientUploadEvent(fh->tempName);
    DataGeography *pDG = NULL;
    ret = HandleClientEvent(pUploadEvent, (char **) &pDG);
    if (ret)
    {
        LOG_ERROR("Failed to upload %s:%s, rc:%d:%s",
                fh->path, fh->tempName, ret, strerror(ret));

        FreeMetaData(&uploadFileMeta, true);
        return -ret;
    }

    time_t now;
    time(&now);
    uploadFileMeta->mMetaData.mStat.st_mtime = now;
    uploadFileMeta->mMetaData.mStat.st_atime = now;

    ret = SetDG(*uploadFileMeta, pDG);
    if (ret)
    {
        LOG_ERROR("Failed to SetDG, path:%s, rc:%d:%s",
                fh->path, ret, strerror(ret));
        FreeMetaData(&uploadFileMeta, true);
        return -ret;
    }

    delete pDG;
    pDG = NULL;

    ret = SetMetaData(*uploadFileMeta);
    if (ret)
    {
        LOG_ERROR("Failed to update the metadata:objId:%s, file:%s, rc:%d:%s",
                fh->objId, fh->path, ret, strerror(ret));
        FreeMetaData(&uploadFileMeta, true);
        return -ret;
    }

    FreeMetaData(&uploadFileMeta);

    fh->flag_need_upload = 0;

    LOG_INFO("Successfull upload %s to %s", fh->path, fh->objId);

    return STATUS_SUCCESS;
}

int CFuse::fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, isdatasync:%d", path, isdatasync);
    int res;

    adfs_filehandler *fh = (adfs_filehandler *) (uintptr_t) fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler, path:%s", path);
        return -ENOENT;
    }

    if (fh->flag_dirty)
    {
        res = flush(path, fi);
        if (res)
        {
            LOG_ERROR("Failed to flush data, path:%s", path);
            return res;
        }
    }

#if !UPLOAD_ON_CLOSE
    //sync data to server
    upload(fh);
#endif
    return 0;
}

void CFuse::releaseFh(adfs_filehandler *fh)
{
    if (fh)
    {
        if (fh->path)
        {
            free(fh->path);
        }
        if (fh->dg)
        {
            free(fh->dg);
        }
        free(fh);
    }
}

int CFuse::release(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("%s", path);
    int res;

    adfs_filehandler *fh = (adfs_filehandler *) (uintptr_t) fi->fh;
    if (fh == NULL)
    {
        LOG_ERROR("No file handler, path:%s", path);
        return -ENOENT;
    }

    if (fh->flag_dirty)
    {
        flush(path, fi);
    }

    if (fh->flag_need_upload)
    {
        upload(fh);
    }

    close(fh->fd);
    ::unlink(fh->tempName);
    releaseFh(fh);

    LOG_DEBUG("Successfully release %s", path);
    return 0;
}

int CFuse::bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    LOG_TRACE("Path:%s, blocksize:%lld", path, (s64_t)blocksize);

    return 0;
}

int CFuse::ioctl(const char *req, int cmd, void *arg, struct fuse_file_info *fi,
        unsigned int flags, void *data)
{
    LOG_TRACE("cmd:%d", cmd);
    return 0;
}

int CFuse::poll(const char *req, struct fuse_file_info *fi,
        struct fuse_pollhandle *ph, unsigned *reventsp)
{
    LOG_TRACE("Enter");
    return 0;
}

int CFuse::CreateRootObj()
{
    const std::string &nameSpace = GlobalMetaCache()->GetDHTNameSpace();

    LOG_INFO("Begin to create root object of %s", nameSpace.c_str());

    int ret = STATUS_SUCCESS;

    char objId[OBJID_LEN] = { 0 };
    ConvertObjId(ROOT_PATH, objId);

    MetaDataStore *metaDataStore = NULL;

    ret = AllocMetaData(objId, &metaDataStore);
    if (ret)
    {
        LOG_ERROR("No memory for root metadata %s", ROOT_PATH);
        return -ENOMEM;
    }

    InitMetaData(*metaDataStore, DEFAULT_DIR_MODE, ROOT_PATH);

    //set parent as himself
    strcpy(metaDataStore->mMetaData.mAdfsNode.mTreeNode.mParentId,
            metaDataStore->mMetaData.mObjId);

    ret = SetMetaData(*metaDataStore);
    if (ret)
    {
        LOG_ERROR("Failed to set root obj's metaData");
        FreeMetaData(&metaDataStore, true);
        return STATUS_INIT_FUSE;
    }

    FreeMetaData(&metaDataStore);

    LOG_INFO("Successfully create root object of %s", nameSpace.c_str());
    return STATUS_SUCCESS;
}

int CFuse::InitRootObj()
{
    MetaDataStore *metaData = NULL;
    int ret = GetMetaDataByPath("/", &metaData, META_READ);
    if (ret == NO_METADATA_OBJ)
    {
        LOG_INFO("No root object, begin to create it");
        ret = CreateRootObj();
        if (ret)
        {
            LOG_ERROR("Failed to create root obj");
            return -ret;
        }
        return STATUS_SUCCESS;
    } else if (ret)
    {
        LOG_ERROR("Failed to init root obj, failed to get root obj metadata");
        return -ret;
    }

    FreeMetaData(&metaData, false, META_READ);

    return STATUS_SUCCESS;
}

void CFuse::Register()
{
    memset(&adfs_oper, 0, sizeof(adfs_oper));
    adfs_oper.destroy = CFuse::destroy;

    adfs_oper.getattr = CFuse::getattr;
    adfs_oper.fgetattr = CFuse::fgetattr;
    adfs_oper.opendir = CFuse::opendir;
    adfs_oper.readdir = CFuse::readdir;
    adfs_oper.releasedir = CFuse::releasedir;

    adfs_oper.mkdir = CFuse::mkdir;
    adfs_oper.rmdir = CFuse::rmdir;
    adfs_oper.create = CFuse::create;
    adfs_oper.open = CFuse::open;
    adfs_oper.read = CFuse::read;
    adfs_oper.write = CFuse::write;
    adfs_oper.flush = CFuse::flush;
    adfs_oper.fsync = CFuse::fsync;
    adfs_oper.release = CFuse::release;
    adfs_oper.link = CFuse::link;
    adfs_oper.readlink = CFuse::readlink;
    adfs_oper.symlink = CFuse::symlink;
    adfs_oper.unlink = CFuse::unlink;
    adfs_oper.utimens = CFuse::utimens;

    adfs_oper.rename = CFuse::rename;
    adfs_oper.chmod = CFuse::chmod;
    adfs_oper.chown = CFuse::chown;
    adfs_oper.truncate = CFuse::truncate;
    adfs_oper.ftruncate = CFuse::ftruncate;
    adfs_oper.statfs = CFuse::statfs;

#if 0
    adfs_oper.access = CFuse::access;
    adfs_oper.mknod = CFuse::mknod;
    adfs_oper.setxattr = CFuse::setxattr;
    adfs_oper.getxattr = CFuse::getxattr;
    adfs_oper.listxattr = CFuse::listxattr;
    adfs_oper.removexattr= CFuse::removexattr;
    adfs_oper.lock = CFuse::lock;
    adfs_oper.getdir = CFuse::getdir;
    adfs_oper.utime = CFuse::utime;
    adfs_oper.fsyncdir = CFuse::fsyncdir;
    adfs_oper.init = CFuse::init;
    adfs_oper.bmap = CFuse::bmap;
    adfs_oper.ioctl = CFuse::ioctl;
    adfs_oper.poll = CFuse::poll;
#endif

    LOG_INFO("Successfully regist all operation function");

}

void* CFuse::init(struct fuse_conn_info *conn)
{
    LOG_INFO("Begin to init");
#if 0
    //this part code has been put in FuseEntry
    int ret = InitRootObj();
    if (ret)
    {
        LOG_ERROR("Failed to init root obj");
        return STATUS_INIT_FUSE;
    }
#endif
}

void CFuse::destroy(void *param)
{
    LOG_INFO("Begin to quit");

    int ret = 0;

    struct statvfs stbuf;
    ret = statfs(ROOT_PATH, &stbuf);

    if (ret)
    {
        LOG_INFO("Successfully update filesystem metrics");
    }

    Finalize();
}

int CFuse::SetPropertis()
{
    const std::string writeThroughKey("CACHE_WRITE_THROUGH");
    std::string writeThroughV = theGlobalProperties()->GetParamValue(
            writeThroughKey, DEFAULT_SESSION);
    if (writeThroughV.size())
    {
        Xlate::strToVal(writeThroughV, mWriteThrough);
    }

    const std::string tempDirKey("TEMPDIR");
    sTempDir = theGlobalProperties()->GetParamValue(tempDirKey,
            DEFAULT_SESSION);
    if (sTempDir.empty())
    {
        LOG_ERROR("Temp dir is empty");
        return STATUS_INIT_FUSE;
    }

    int ret = ::access(sTempDir.c_str(), R_OK | W_OK | X_OK);
    if (ret)
    {
        LOG_ERROR("Failed to access %s, rc:%d:%s",
                sTempDir.c_str(), errno, strerror(errno));
        return STATUS_INIT_FUSE;
    }

    return STATUS_SUCCESS;
}

int CFuse::FuseEntry()
{
    int ret = SetPropertis();
    if (ret)
    {
        LOG_ERROR("Failed to get properties");
        return -STATUS_INIT_FUSE;
    }
    ret = InitRootObj();
    if (ret)
    {
        LOG_ERROR("Failed to init root obj");
        return -STATUS_INIT_FUSE;
    }

    Register();

    char *argv[10] = { 0 };
    int argc = 0;
    ret = InitMountArgs(argc, argv);
    if (ret)
    {
        LOG_ERROR("Failed to init mount args");
        return STATUS_INIT_FUSE;
    }

    ret = fuse_main(argc, argv, &adfs_oper, NULL);
    if (ret)
    {
        LOG_ERROR("Failed to init fuse client");
        return ret;
    }

    FreeMountArgs(argc, argv);

    LOG_INFO("Begin to quit fuse loop");
    return 0;

}

/*
 general options:
 -o opt,[opt...]        mount options
 -h   --help            print help
 -V   --version         print version

 FUSE options:
 -d   -o debug          enable debug output (implies -f)
 -f                     foreground operation
 -s                     disable multi-threaded operation

 -o allow_other         allow access to other users
 -o allow_root          allow access to root
 -o nonempty            allow mounts over non-empty file/dir
 -o default_permissions enable permission checking by kernel
 -o fsname=NAME         set filesystem name
 -o subtype=NAME        set filesystem type
 -o large_read          issue large read requests (2.4 only)
 -o max_read=N          set maximum size of read requests

 -o hard_remove         immediate removal (don't hide files)
 -o use_ino             let filesystem set inode numbers
 -o readdir_ino         try to fill in d_ino in readdir
 -o direct_io           use direct I/O
 -o kernel_cache        cache files in kernel
 -o [no]auto_cache      enable caching based on modification times (off)
 -o umask=M             set file permissions (octal)
 -o uid=N               set file owner
 -o gid=N               set file group
 -o entry_timeout=T     cache timeout for names (1.0s)
 -o negative_timeout=T  cache timeout for deleted names (0.0s)
 -o attr_timeout=T      cache timeout for attributes (1.0s)
 -o ac_attr_timeout=T   auto cache timeout for attributes (attr_timeout)
 -o intr                allow requests to be interrupted
 -o intr_signal=NUM     signal to send on interrupt (10)
 -o modules=M1[:M2...]  names of modules to push onto filesystem stack

 -o max_write=N         set maximum size of write requests
 -o max_readahead=N     set maximum readahead
 -o async_read          perform reads asynchronously (default)
 -o sync_read           perform reads synchronously
 -o atomic_o_trunc      enable atomic open+truncate support
 -o big_writes          enable larger than 4kB writes
 -o no_remote_lock      disable remote file locking

 Module options:

 [subdir]
 -o subdir=DIR           prepend this directory to all paths (mandatory)
 -o [no]rellinks         transform absolute symlinks to relative

 [iconv]
 -o from_code=CHARSET   original encoding of file names (default: UTF-8)
 -o to_code=CHARSET      new encoding of the file names (default: GB18030)
 */
