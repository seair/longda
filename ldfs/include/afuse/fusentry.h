// __CR__
// Copyright (c) 2008-2010 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__


#ifndef _FUSE_ENTRY_H_
#define _FUSE_ENTRY_H_

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/types.h>
#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "metadata.h"
#include "metacache.h"

class CFuse
{
public:
    static int  FuseEntry();
    static void Register();
    static int  SetPropertis();
    
public:
    /* Attention please:
     *       All return value in ourself is positive
     *       But return value to fuse need to be negative,
     *           so we transform the return to negative in all fuse interfaces
     */
    /*
     * fuse operations begin
     * please refer to fuse spec's fuse_operations for every function meaning
     *
     */
    static int getattr(const char *, struct stat *);
    static int readlink(const char *, char *, size_t);
    static int getdir(const char *, fuse_dirh_t, fuse_dirfil_t);
    static int mknod(const char *, mode_t, dev_t);
    static int mkdir(const char *, mode_t);
    static int unlink(const char *);
    static int rmdir(const char *);
    static int symlink(const char *, const char *);
    static int rename(const char *, const char *);
    static int link(const char *, const char *);
    static int chmod(const char *, mode_t);
    static int chown(const char *, uid_t, gid_t);
    static int truncate(const char *, off_t);
    static int utime(const char *, struct utimbuf *);
    static int open(const char *, struct fuse_file_info *);
    static int read(const char *, char *, size_t, off_t, struct fuse_file_info *);
    static int write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
    static int statfs(const char *, struct statvfs *);
    static int flush(const char *, struct fuse_file_info *);
    static int release(const char *, struct fuse_file_info *);
    static int fsync(const char *, int, struct fuse_file_info *);
    static int setxattr(const char *, const char *, const char *, size_t, int);
    static int getxattr(const char *, const char *, char *, size_t);
    static int listxattr(const char *, char *, size_t);
    static int removexattr(const char *, const char *);
    static int opendir(const char *, struct fuse_file_info *);
    static int readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    static int releasedir(const char *, struct fuse_file_info *);
    static int fsyncdir(const char *, int, struct fuse_file_info *);
    static void* init(struct fuse_conn_info *conn);
    static void destroy(void *);
    static int access(const char *, int);
    static int create(const char *, mode_t, struct fuse_file_info *);
    static int ftruncate(const char *, off_t, struct fuse_file_info *);
    static int fgetattr(const char *, struct stat *, struct fuse_file_info *);
    static int lock(const char *, struct fuse_file_info *, int cmd, struct flock *);
    static int utimens(const char *, const struct timespec tv[2]);
    static int bmap(const char *, size_t blocksize, uint64_t *idx);
    static int ioctl(const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
    static int poll(const char *, struct fuse_file_info *,  struct fuse_pollhandle *ph, unsigned *reventsp);
    /*
     * fuse operation end
     * please refer to fuse spec's fuse_operations for every function meaning.
     *
     */

private:
    struct adfs_dirp {
        MetaDataStore *metaData;
        off_t          offset;
        FILE          *dirContentFd;
        char           dirContentName[FILENAME_LENGTH_MAX];
        struct flock   dirContentLock;
        bool           isLocked;
    };

    typedef struct _adfs_filehandler{
        char          *path;
        char           tempName[FILENAME_LENGTH_MAX];
        char           objId[OBJID_LEN];
        char           parentOID[OBJID_LEN];
        DataGeography *dg;
        int            fd;             //open fd
        unsigned int   flag_dirty : 1;
        unsigned int   flag_need_upload :1;
        unsigned int   flag_reserved : 31;  //Reserved flags, don't set
    }adfs_filehandler;

    //In the implementation, rename is simliar with link, so put it here
    typedef enum{
        SOFT_LINK_TYPE   = 0,
        HARD_LINK_TYPE,
        RENAME_TYPE
    }LinkType;

    class CLinkPrecheck
    {
    public:
        CLinkPrecheck(LinkType type):mType(type), mFromeMeta(NULL){};

    public:
        LinkType       mType;
        MetaDataStore *mFromeMeta;
        std::string    mFromOID;
        std::string    mToFileName;
        std::string    mToParentPath;
    };

private:
    //utility functions

    /**
     * Root obj operation fuction
     */
    static int  InitRootObj();

    static int  CreateRootObj();

    /**
     * Upload file encapsulate function
     */
    static int  upload_new_file(adfs_filehandler *fh);

    static int  update_old_file(adfs_filehandler *fh, MetaDataStore *uploadFileMeta);

    static int  upload(adfs_filehandler *fh);

    static void releaseFh(adfs_filehandler *fh);

    /**
     * Metadata operaiton functions
     */
    static void ConvertObjId(const char *path, char *objId);

    static void InitMetaData(MetaDataStore &metaDataStore, mode_t    st_mode, const char *path);

    static int  DelMetaData(MetaDataStore &metaDataStore);

    static int  SetMetaData(MetaDataStore &metaDataStore);

    static int  GetMetaDataByID(const char *objId, MetaDataStore **metaData, MetaOp op = META_WRITE);

    static int  GetMetaDataByPath(const char *path, MetaDataStore **metaData, MetaOp op = META_WRITE);

    static int  AllocMetaData(const char *objId, MetaDataStore **metaData);

    static void FreeMetaData(MetaDataStore **metaData, bool forceFree = false, MetaOp op = META_WRITE);

    /**
     * Insert metadata
     */

    static int  InsertTreeMeta(MetaDataStore &metaDataStore, const char *parentOID);

    static int  InsertHardLink(MetaDataStore &metaDataStore, const char *hardlinkOID);

    static int  InsertNodeMeta(MetaDataStore &metaDataStore, 
                const char *parentOID, 
                const char *hardlinkOID,
                const char *softLinkPath);

    /**
     * Remove metadata
     */

    static int  RmTreeMeta(MetaDataStore &rmDirMeta);

    static int  RmNodeMeta(MetaDataStore &rmDirMeta);

    static int  RmNodeMeta(const char *path);

    static void CopyMetaData(MetaDataStore &target, const MetaDataStore &source);

    static int  RmData(MetaDataStore *pRmMeta);

    static int  Rm(const char *path);


    /**
     * creat link -- CreateLink
     * create Hard Link -- CreateHardLink
     * create Soft Link -- CreateSoftLink
     */
    static int  LinkPrecheck(const char *from, const char *to, CLinkPrecheck &linkPrecheck);
    
    static int  CreateHardLink(const char *from, const char *to, CLinkPrecheck &linkPrecheck);
    
    static int  CreateSoftLink(const char *from, const char *to, CLinkPrecheck &linkPrecheck);

    static int  CreateHardLinkDG(MetaDataStore *fromMeta);

    
    /**
     * utility function
     */

    static int  releasedir(struct adfs_dirp *d);

    static int  InitFsMetrics(char **dhtValue, int *dhtValueLen);

    static int  FileLock(const char *blockId, int cmd, struct flock *lock);

    static int GenerateTempFile(const char *objId, char *tempName);

    static int SetDG(MetaDataStore &uploadFileMeta, DataGeography     *pDG);



private:
    static std::string sTempDir;

    static struct fuse_operations adfs_oper;

    static bool        mWriteThrough;

};

#endif  // _FUSE_ENTRY_H_

