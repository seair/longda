/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` -lulockmgr fusexmp_fh.c -o fusexmp_fh
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

#include "defs.h"
#include "log.h"

#include "util.h"

extern int  InitMountArgs(int &argc, char *argv[]);
extern void FreeMountArgs(int argc, char *argv[]);

static int xmp_getattr(const char *path, struct stat *stbuf)
{
    
    int res;

    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;

    LOG_TRACE("path:%s, st_dev:0x%x, st_ino:0x%llx, st_mode:0x%x, "
        "st_nlink:%d, st_uid:%d, st_gid:%d, st_rdev:0x%x, "
        "st_size:%lld, st_blksize:%d, st_blocks:%d", 
        path, (int)stbuf->st_dev, (s64_t)stbuf->st_ino, (int)stbuf->st_mode,
        (int)stbuf->st_nlink, (int)stbuf->st_uid, (int)stbuf->st_gid, (int)stbuf->st_rdev,
        (s64_t)stbuf->st_size, (int)stbuf->st_blksize, (int)stbuf->st_blocks);
    return 0;
}

static int xmp_fgetattr(const char *path, struct stat *stbuf,
            struct fuse_file_info *fi)
{
    int res;

    (void) path;

    res = fstat(fi->fh, stbuf);
    if (res == -1)
        return -errno;

    LOG_TRACE("path:%s, st_dev:0x%x, st_ino:0x%llx, st_mode:0x%x, "
        "st_nlink:%d, st_uid:%d, st_gid:%d, st_rdev:0x%x, "
        "st_size:%lld, st_blksize:%d, st_blocks:%d", 
        path, (int)stbuf->st_dev, (s64_t)stbuf->st_ino, (int)stbuf->st_mode,
        (int)stbuf->st_nlink, (int)stbuf->st_uid, (int)stbuf->st_gid, (int)stbuf->st_rdev,
        (s64_t)stbuf->st_size, (int)stbuf->st_blksize, (int)stbuf->st_blocks);

    return 0;
}

static int xmp_access(const char *path, int mask)
{
    LOG_TRACE("Path:%s, mask:0x%x", path, mask);
    int res;

    res = access(path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
    
    int res;

    res = readlink(path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    LOG_TRACE("Path:%s, size:%llu, buf:%s", path, (u64_t)size, buf);
    return 0;
}

struct xmp_dirp {
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s", path);
    int res;
    struct xmp_dirp *d = (struct xmp_dirp *)malloc(sizeof(struct xmp_dirp));
    if (d == NULL)
        return -ENOMEM;

    d->dp = opendir(path);
    if (d->dp == NULL) {
        res = -errno;
        free(d);
        return res;
    }
    d->offset = 0;
    d->entry = NULL;

    fi->fh = (unsigned long) d;
    return 0;
}

static inline struct xmp_dirp *get_dirp(struct fuse_file_info *fi)
{
    return (struct xmp_dirp *) (uintptr_t) fi->fh;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s", path);
    struct xmp_dirp *d = get_dirp(fi);

    (void) path;
    if (offset != d->offset) {
        LOG_DEBUG("Offset not equal, offset:%llu, d->offset:%llu",
            (u64_t)offset, (u64_t)d->offset);
        seekdir(d->dp, offset);
        d->entry = NULL;
        d->offset = offset;
    }
    while (1) {
        struct stat st;
        off_t nextoff;

        if (!d->entry) {
            d->entry = readdir(d->dp);
            if (!d->entry)
                break;
        }

        memset(&st, 0, sizeof(st));
        st.st_ino = d->entry->d_ino;
        st.st_mode = d->entry->d_type << 12;
        nextoff = telldir(d->dp);
        LOG_DEBUG("d_name:%s, st_mode:0x%x, st_ino:0x%llx, nextoff:0x%llx",
            d->entry->d_name, (int)d->entry->d_type, 
            (s64_t)d->entry->d_ino, (s64_t)nextoff);
        if (filler(buf, d->entry->d_name, &st, nextoff))
            break;

        d->entry = NULL;
        d->offset = nextoff;
    }

    return 0;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s", path);
    
    struct xmp_dirp *d = get_dirp(fi);
    (void) path;
    closedir(d->dp);
    free(d);
    return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    LOG_TRACE("Path:%s, mode:0x%x, dev:0x%x", path, (int)mode, (int)rdev);
    
    int res;

    if (S_ISFIFO(mode))
        res = mkfifo(path, mode);
    else
        res = mknod(path, mode, rdev);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
    LOG_TRACE("Path:%s, mode:0x%x", path, (int)mode);
    
    int res;

    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_unlink(const char *path)
{
    LOG_TRACE("Path:%s", path);
    
    int res;

    res = unlink(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rmdir(const char *path)
{
    LOG_TRACE("path:%s", path);
    
    int res;

    res = rmdir(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
    LOG_TRACE("from:%s, to:%s", from, to);
    
    int res;

    res = symlink(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rename(const char *from, const char *to)
{
    LOG_TRACE("from:%s, to:%s", from, to);
    
    int res;

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_link(const char *from, const char *to)
{
    LOG_TRACE("from:%s, to:%s", from, to);
    
    int res;

    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
    LOG_TRACE("Path:%s, mode:0x%x", path, (int)mode);
    
    int res;

    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
    LOG_TRACE("Path:%s, uid:%d, gid:%d", path, (int)uid, (int)gid);
    
    int res;

    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
    LOG_TRACE("Path:%s, size:%lld", path, (s64_t)size);
    
    int res;

    res = truncate(path, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_ftruncate(const char *path, off_t size,
             struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, size:%lld", path, (s64_t)size);
    
    int res;

    (void) path;

    res = ftruncate(fi->fh, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
    LOG_TRACE("path:%s", path);
    
    int res;
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    res = utimes(path, tv);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, mode:0x%x", path, (int)mode);
    
    int fd;

    fd = open(path, fi->flags, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("path:%s, flags:0x%x", path, fi->flags);
    
    int fd;

    fd = open(path, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, size:%lld, offset:%lld", path, (s64_t)size, (s64_t)offset);
    
    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi)
{
    LOG_TRACE("Path:%s, size:%lld, offset:%lld", path, (s64_t)size, (s64_t)offset);
    
    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
    LOG_TRACE("path:%s", path);
    
    int res;

    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("path:%s", path);
    
    int res;

    (void) path;
    /* This is called from every close on an open file, so call the
       close on the underlying filesystem.  But since flush may be
       called multiple times for an open file, this must not really
       close the file.  This is important if used on a network
       filesystem like NFS which flush the data/metadata on close() */
    res = close(dup(fi->fh));
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
    LOG_TRACE("path:%s", path);
    
    (void) path;
    close(fi->fh);

    return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
             struct fuse_file_info *fi)
{
    LOG_TRACE("path:%s, fd:%d", path, (int)fi->fh);
    
    int res;
    (void) path;

#ifndef HAVE_FDATASYNC
    (void) isdatasync;
#else
    if (isdatasync)
        res = fdatasync(fi->fh);
    else
#endif
        res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
            size_t size, int flags)
{
    LOG_TRACE("Path:%s, name:%s, value:%s, size:%lld, flag:%d",
        path, name, value, (s64_t)size, flags);
    
    int res = lsetxattr(path, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
            size_t size)
{
    LOG_TRACE("Path:%s, name:%s, size:%lld",
        path, name, (s64_t)size);
    
    int res = lgetxattr(path, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    LOG_TRACE("Path:%s, size:%lld", path, (s64_t)size);
    
    int res = llistxattr(path, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
    LOG_TRACE("Path:%s, name:%s", path, name);
    
    int res = lremovexattr(path, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
            struct flock *lock)
{
    LOG_TRACE("Path:%s, cmd:%d", path, cmd);
    
    (void) path;
#if 0

    return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
               sizeof(fi->lock_owner));
#endif
    return 0;
}

extern void Finalize();
static void xmp_destroy(void *param)
{
    LOG_INFO("Begin to quit");
    Finalize();
}

static struct fuse_operations xmp_oper;

int xmp_register()
{
    memset(&xmp_oper, 0, sizeof(xmp_oper));
    
    xmp_oper.getattr    = xmp_getattr;
    xmp_oper.fgetattr   = xmp_fgetattr;
    xmp_oper.access     = xmp_access;
    xmp_oper.readlink   = xmp_readlink;
    xmp_oper.opendir    = xmp_opendir;
    xmp_oper.readdir    = xmp_readdir;
    xmp_oper.releasedir = xmp_releasedir;
    xmp_oper.mknod      = xmp_mknod;
    xmp_oper.mkdir      = xmp_mkdir;
    xmp_oper.symlink    = xmp_symlink;
    xmp_oper.unlink     = xmp_unlink;
    xmp_oper.rmdir      = xmp_rmdir;
    xmp_oper.rename     = xmp_rename;
    xmp_oper.link       = xmp_link;
    xmp_oper.chmod      = xmp_chmod;
    xmp_oper.chown      = xmp_chown;
    xmp_oper.truncate   = xmp_truncate;
    xmp_oper.ftruncate  = xmp_ftruncate;
    xmp_oper.utimens    = xmp_utimens;
    xmp_oper.create     = xmp_create;
    xmp_oper.open       = xmp_open;
    xmp_oper.read       = xmp_read;
    xmp_oper.write      = xmp_write;
    xmp_oper.statfs     = xmp_statfs;
    xmp_oper.flush      = xmp_flush;
    xmp_oper.release    = xmp_release;
    xmp_oper.fsync      = xmp_fsync;
#ifdef HAVE_SETXATTR
    xmp_oper.setxattr   = xmp_setxattr;
    xmp_oper.getxattr   = xmp_getxattr;
    xmp_oper.listxattr  = xmp_listxattr;
    xmp_oper.removexattr    = xmp_removexattr;
#endif
    xmp_oper.lock       = xmp_lock;
    xmp_oper.destroy    = xmp_destroy;

    xmp_oper.flag_nullpath_ok = 0;
}

int xmp_fusentry()
{
    xmp_register();
    
    char *argv[10] = {0};
    int argc = 0;
    int ret = InitMountArgs(argc, argv);
    if (ret)
    {
        LOG_ERROR("Failed to init mount args");
        return STATUS_INIT_FUSE;
    }
    ret = fuse_main(argc, argv, &xmp_oper, NULL);

    FreeMountArgs(argc, argv);

    return ret;
}
