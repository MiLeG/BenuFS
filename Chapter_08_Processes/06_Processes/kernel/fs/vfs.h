/*! Virtual file system */
#pragma once

#include "lfs.h"

int k_vfs_is_file_open(descriptor_t* desc);
int k_vfs_open_file(char* pathname, int flags, mode_t mode, descriptor_t* desc);
int k_vfs_close_file(descriptor_t* desc);
int k_vfs_read_write(descriptor_t* desc, void* buffer, size_t size, int op);

#define _K_VFS_C_ //TODO - remove define
#ifdef _K_VFS_C_

#include "../thread.h"
#include "types/time.h"

#define KTYPE_FILE    (1<<10)

/*! Maximum length of VFS path prefix (not including zero terminator)  */
#define LFS_PREFIX_MAX 15

/*! Maximum number of registered filesystems */
#define VFS_MAX_COUNT 5

/**
 * Maximum number of (global) file descriptors.
 */
#define VFS_MAX_FDS         64

typedef struct kfile_desc {
	id_t id;    // kernel object id
	int vfs_fd; // lfs file descriptor index
	int flags;
} kfile_desc_t;

#endif /* _K_VFS_C_ */
