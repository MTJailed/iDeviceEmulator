/*
 * Virtio 9p Posix callback
 *
 * Copyright IBM, Corp. 2010
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */
#include "virtio.h"
#include "virtio-9p.h"
#include "virtio-9p-xattr.h"
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <attr/xattr.h>


static int local_lstat(FsContext *fs_ctx, const char *path, struct stat *stbuf)
{
    int err;
    err =  lstat(rpath(fs_ctx, path), stbuf);
    if (err) {
        return err;
    }
    if (fs_ctx->fs_sm == SM_MAPPED) {
        /* Actual credentials are part of extended attrs */
        uid_t tmp_uid;
        gid_t tmp_gid;
        mode_t tmp_mode;
        dev_t tmp_dev;
        if (getxattr(rpath(fs_ctx, path), "user.virtfs.uid", &tmp_uid,
                    sizeof(uid_t)) > 0) {
            stbuf->st_uid = tmp_uid;
        }
        if (getxattr(rpath(fs_ctx, path), "user.virtfs.gid", &tmp_gid,
                    sizeof(gid_t)) > 0) {
            stbuf->st_gid = tmp_gid;
        }
        if (getxattr(rpath(fs_ctx, path), "user.virtfs.mode", &tmp_mode,
                    sizeof(mode_t)) > 0) {
            stbuf->st_mode = tmp_mode;
        }
        if (getxattr(rpath(fs_ctx, path), "user.virtfs.rdev", &tmp_dev,
                        sizeof(dev_t)) > 0) {
                stbuf->st_rdev = tmp_dev;
        }
    }
    return err;
}

static int local_set_xattr(const char *path, FsCred *credp)
{
    int err;
    if (credp->fc_uid != -1) {
        err = setxattr(path, "user.virtfs.uid", &credp->fc_uid, sizeof(uid_t),
                0);
        if (err) {
            return err;
        }
    }
    if (credp->fc_gid != -1) {
        err = setxattr(path, "user.virtfs.gid", &credp->fc_gid, sizeof(gid_t),
                0);
        if (err) {
            return err;
        }
    }
    if (credp->fc_mode != -1) {
        err = setxattr(path, "user.virtfs.mode", &credp->fc_mode,
                sizeof(mode_t), 0);
        if (err) {
            return err;
        }
    }
    if (credp->fc_rdev != -1) {
        err = setxattr(path, "user.virtfs.rdev", &credp->fc_rdev,
                sizeof(dev_t), 0);
        if (err) {
            return err;
        }
    }
    return 0;
}

static int local_post_create_passthrough(FsContext *fs_ctx, const char *path,
        FsCred *credp)
{
    if (chmod(rpath(fs_ctx, path), credp->fc_mode & 07777) < 0) {
        return -1;
    }
    if (lchown(rpath(fs_ctx, path), credp->fc_uid, credp->fc_gid) < 0) {
        /*
         * If we fail to change ownership and if we are
         * using security model none. Ignore the error
         */
        if (fs_ctx->fs_sm != SM_NONE) {
            return -1;
        }
    }
    return 0;
}

static ssize_t local_readlink(FsContext *fs_ctx, const char *path,
        char *buf, size_t bufsz)
{
    ssize_t tsize = -1;
    if (fs_ctx->fs_sm == SM_MAPPED) {
        int fd;
        fd = open(rpath(fs_ctx, path), O_RDONLY);
        if (fd == -1) {
            return -1;
        }
        do {
            tsize = read(fd, (void *)buf, bufsz);
        } while (tsize == -1 && errno == EINTR);
        close(fd);
        return tsize;
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        tsize = readlink(rpath(fs_ctx, path), buf, bufsz);
    }
    return tsize;
}

static int local_close(FsContext *ctx, int fd)
{
    return close(fd);
}

static int local_closedir(FsContext *ctx, DIR *dir)
{
    return closedir(dir);
}

static int local_open(FsContext *ctx, const char *path, int flags)
{
    return open(rpath(ctx, path), flags);
}

static DIR *local_opendir(FsContext *ctx, const char *path)
{
    return opendir(rpath(ctx, path));
}

static void local_rewinddir(FsContext *ctx, DIR *dir)
{
    return rewinddir(dir);
}

static off_t local_telldir(FsContext *ctx, DIR *dir)
{
    return telldir(dir);
}

static struct dirent *local_readdir(FsContext *ctx, DIR *dir)
{
    return readdir(dir);
}

static void local_seekdir(FsContext *ctx, DIR *dir, off_t off)
{
    return seekdir(dir, off);
}

static ssize_t local_preadv(FsContext *ctx, int fd, const struct iovec *iov,
                            int iovcnt, off_t offset)
{
#ifdef CONFIG_PREADV
    return preadv(fd, iov, iovcnt, offset);
#else
    int err = lseek(fd, offset, SEEK_SET);
    if (err == -1) {
        return err;
    } else {
        return readv(fd, iov, iovcnt);
    }
#endif
}

static ssize_t local_pwritev(FsContext *ctx, int fd, const struct iovec *iov,
                            int iovcnt, off_t offset)
{
#ifdef CONFIG_PREADV
    return pwritev(fd, iov, iovcnt, offset);
#else
    int err = lseek(fd, offset, SEEK_SET);
    if (err == -1) {
        return err;
    } else {
        return writev(fd, iov, iovcnt);
    }
#endif
}

static int local_chmod(FsContext *fs_ctx, const char *path, FsCred *credp)
{
    if (fs_ctx->fs_sm == SM_MAPPED) {
        return local_set_xattr(rpath(fs_ctx, path), credp);
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        return chmod(rpath(fs_ctx, path), credp->fc_mode);
    }
    return -1;
}

static int local_mknod(FsContext *fs_ctx, const char *path, FsCred *credp)
{
    int err = -1;
    int serrno = 0;

    /* Determine the security model */
    if (fs_ctx->fs_sm == SM_MAPPED) {
        err = mknod(rpath(fs_ctx, path), SM_LOCAL_MODE_BITS|S_IFREG, 0);
        if (err == -1) {
            return err;
        }
        local_set_xattr(rpath(fs_ctx, path), credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        err = mknod(rpath(fs_ctx, path), credp->fc_mode, credp->fc_rdev);
        if (err == -1) {
            return err;
        }
        err = local_post_create_passthrough(fs_ctx, path, credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    }
    return err;

err_end:
    remove(rpath(fs_ctx, path));
    errno = serrno;
    return err;
}

static int local_mkdir(FsContext *fs_ctx, const char *path, FsCred *credp)
{
    int err = -1;
    int serrno = 0;

    /* Determine the security model */
    if (fs_ctx->fs_sm == SM_MAPPED) {
        err = mkdir(rpath(fs_ctx, path), SM_LOCAL_DIR_MODE_BITS);
        if (err == -1) {
            return err;
        }
        credp->fc_mode = credp->fc_mode|S_IFDIR;
        err = local_set_xattr(rpath(fs_ctx, path), credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        err = mkdir(rpath(fs_ctx, path), credp->fc_mode);
        if (err == -1) {
            return err;
        }
        err = local_post_create_passthrough(fs_ctx, path, credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    }
    return err;

err_end:
    remove(rpath(fs_ctx, path));
    errno = serrno;
    return err;
}

static int local_fstat(FsContext *fs_ctx, int fd, struct stat *stbuf)
{
    int err;
    err = fstat(fd, stbuf);
    if (err) {
        return err;
    }
    if (fs_ctx->fs_sm == SM_MAPPED) {
        /* Actual credentials are part of extended attrs */
        uid_t tmp_uid;
        gid_t tmp_gid;
        mode_t tmp_mode;
        dev_t tmp_dev;

        if (fgetxattr(fd, "user.virtfs.uid", &tmp_uid, sizeof(uid_t)) > 0) {
            stbuf->st_uid = tmp_uid;
        }
        if (fgetxattr(fd, "user.virtfs.gid", &tmp_gid, sizeof(gid_t)) > 0) {
            stbuf->st_gid = tmp_gid;
        }
        if (fgetxattr(fd, "user.virtfs.mode", &tmp_mode, sizeof(mode_t)) > 0) {
            stbuf->st_mode = tmp_mode;
        }
        if (fgetxattr(fd, "user.virtfs.rdev", &tmp_dev, sizeof(dev_t)) > 0) {
                stbuf->st_rdev = tmp_dev;
        }
    }
    return err;
}

static int local_open2(FsContext *fs_ctx, const char *path, int flags,
        FsCred *credp)
{
    int fd = -1;
    int err = -1;
    int serrno = 0;

    /* Determine the security model */
    if (fs_ctx->fs_sm == SM_MAPPED) {
        fd = open(rpath(fs_ctx, path), flags, SM_LOCAL_MODE_BITS);
        if (fd == -1) {
            return fd;
        }
        credp->fc_mode = credp->fc_mode|S_IFREG;
        /* Set cleint credentials in xattr */
        err = local_set_xattr(rpath(fs_ctx, path), credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        fd = open(rpath(fs_ctx, path), flags, credp->fc_mode);
        if (fd == -1) {
            return fd;
        }
        err = local_post_create_passthrough(fs_ctx, path, credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    }
    return fd;

err_end:
    close(fd);
    remove(rpath(fs_ctx, path));
    errno = serrno;
    return err;
}


static int local_symlink(FsContext *fs_ctx, const char *oldpath,
        const char *newpath, FsCred *credp)
{
    int err = -1;
    int serrno = 0;

    /* Determine the security model */
    if (fs_ctx->fs_sm == SM_MAPPED) {
        int fd;
        ssize_t oldpath_size, write_size;
        fd = open(rpath(fs_ctx, newpath), O_CREAT|O_EXCL|O_RDWR,
                SM_LOCAL_MODE_BITS);
        if (fd == -1) {
            return fd;
        }
        /* Write the oldpath (target) to the file. */
        oldpath_size = strlen(oldpath) + 1;
        do {
            write_size = write(fd, (void *)oldpath, oldpath_size);
        } while (write_size == -1 && errno == EINTR);

        if (write_size != oldpath_size) {
            serrno = errno;
            close(fd);
            err = -1;
            goto err_end;
        }
        close(fd);
        /* Set cleint credentials in symlink's xattr */
        credp->fc_mode = credp->fc_mode|S_IFLNK;
        err = local_set_xattr(rpath(fs_ctx, newpath), credp);
        if (err == -1) {
            serrno = errno;
            goto err_end;
        }
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        err = symlink(oldpath, rpath(fs_ctx, newpath));
        if (err) {
            return err;
        }
        err = lchown(rpath(fs_ctx, newpath), credp->fc_uid, credp->fc_gid);
        if (err == -1) {
            /*
             * If we fail to change ownership and if we are
             * using security model none. Ignore the error
             */
            if (fs_ctx->fs_sm != SM_NONE) {
                serrno = errno;
                goto err_end;
            } else
                err = 0;
        }
    }
    return err;

err_end:
    remove(rpath(fs_ctx, newpath));
    errno = serrno;
    return err;
}

static int local_link(FsContext *ctx, const char *oldpath, const char *newpath)
{
    char *tmp = qemu_strdup(rpath(ctx, oldpath));
    int err, serrno = 0;

    if (tmp == NULL) {
        return -ENOMEM;
    }

    err = link(tmp, rpath(ctx, newpath));
    if (err == -1) {
        serrno = errno;
    }

    qemu_free(tmp);

    if (err == -1) {
        errno = serrno;
    }

    return err;
}

static int local_truncate(FsContext *ctx, const char *path, off_t size)
{
    return truncate(rpath(ctx, path), size);
}

static int local_rename(FsContext *ctx, const char *oldpath,
                        const char *newpath)
{
    char *tmp;
    int err;

    tmp = qemu_strdup(rpath(ctx, oldpath));

    err = rename(tmp, rpath(ctx, newpath));
    if (err == -1) {
        int serrno = errno;
        qemu_free(tmp);
        errno = serrno;
    } else {
        qemu_free(tmp);
    }

    return err;

}

static int local_chown(FsContext *fs_ctx, const char *path, FsCred *credp)
{
    if ((credp->fc_uid == -1 && credp->fc_gid == -1) ||
            (fs_ctx->fs_sm == SM_PASSTHROUGH)) {
        return lchown(rpath(fs_ctx, path), credp->fc_uid, credp->fc_gid);
    } else if (fs_ctx->fs_sm == SM_MAPPED) {
        return local_set_xattr(rpath(fs_ctx, path), credp);
    } else if ((fs_ctx->fs_sm == SM_PASSTHROUGH) ||
               (fs_ctx->fs_sm == SM_NONE)) {
        return lchown(rpath(fs_ctx, path), credp->fc_uid, credp->fc_gid);
    }
    return -1;
}

static int local_utimensat(FsContext *s, const char *path,
                           const struct timespec *buf)
{
    return qemu_utimensat(AT_FDCWD, rpath(s, path), buf, AT_SYMLINK_NOFOLLOW);
}

static int local_remove(FsContext *ctx, const char *path)
{
    return remove(rpath(ctx, path));
}

static int local_fsync(FsContext *ctx, int fd, int datasync)
{
    if (datasync) {
        return qemu_fdatasync(fd);
    } else {
        return fsync(fd);
    }
}

static int local_statfs(FsContext *s, const char *path, struct statfs *stbuf)
{
   return statfs(rpath(s, path), stbuf);
}

static ssize_t local_lgetxattr(FsContext *ctx, const char *path,
                               const char *name, void *value, size_t size)
{
    return v9fs_get_xattr(ctx, path, name, value, size);
}

static ssize_t local_llistxattr(FsContext *ctx, const char *path,
                                void *value, size_t size)
{
    return v9fs_list_xattr(ctx, path, value, size);
}

static int local_lsetxattr(FsContext *ctx, const char *path, const char *name,
                           void *value, size_t size, int flags)
{
    return v9fs_set_xattr(ctx, path, name, value, size, flags);
}

static int local_lremovexattr(FsContext *ctx,
                              const char *path, const char *name)
{
    return v9fs_remove_xattr(ctx, path, name);
}


FileOperations local_ops = {
    .lstat = local_lstat,
    .readlink = local_readlink,
    .close = local_close,
    .closedir = local_closedir,
    .open = local_open,
    .opendir = local_opendir,
    .rewinddir = local_rewinddir,
    .telldir = local_telldir,
    .readdir = local_readdir,
    .seekdir = local_seekdir,
    .preadv = local_preadv,
    .pwritev = local_pwritev,
    .chmod = local_chmod,
    .mknod = local_mknod,
    .mkdir = local_mkdir,
    .fstat = local_fstat,
    .open2 = local_open2,
    .symlink = local_symlink,
    .link = local_link,
    .truncate = local_truncate,
    .rename = local_rename,
    .chown = local_chown,
    .utimensat = local_utimensat,
    .remove = local_remove,
    .fsync = local_fsync,
    .statfs = local_statfs,
    .lgetxattr = local_lgetxattr,
    .llistxattr = local_llistxattr,
    .lsetxattr = local_lsetxattr,
    .lremovexattr = local_lremovexattr,
};
