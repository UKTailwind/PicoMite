/* sys/statvfs.h: a file system's size and free space for a loadable program (tools/sdk), which newlib does not have
   for this target. MIT License, as pmimg.h. statvfs answers ENOSYS (syscalls.c): the services do not give the
   drive's free space. */
#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>                          /* (fsblkcnt_t, fsfilcnt_t) */

struct statvfs {
    unsigned long f_bsize, f_frsize;
    fsblkcnt_t f_blocks, f_bfree, f_bavail;
    fsfilcnt_t f_files, f_ffree, f_favail;
    unsigned long f_fsid, f_flag, f_namemax;
};

int statvfs(const char *path, struct statvfs *buf);

#ifdef __cplusplus
}
#endif
#endif
