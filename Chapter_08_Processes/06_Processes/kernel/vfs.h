/*! Virtual file system */
#pragma once

/*! VFS object */
typedef struct _kvfs_t_ {

	int (* open)(char* pathname, int flags, mode_t mode, descriptor_t* desc);

	int (* close)(descriptor_t* desc);

	int (* read_write)(descriptor_t* desc, void* buffer, size_t size, int op);
} kvfs_t;

int k_vfs_register(const char* base_path, const kvfs_t* vfs);
int k_vfs_unregister(const char* base_path);

int k_fs_is_file_open(descriptor_t* desc);
int k_fs_open_file(char* pathname, int flags, mode_t mode, descriptor_t* desc);
int k_fs_close_file(descriptor_t* desc);
int k_fs_read_write(descriptor_t* desc, void* buffer, size_t size, int op);

#define _K_VFS_C_ //TODO - remove define
#ifdef _K_VFS_C_

#include "thread.h"
#include <types/time.h>

#define KTYPE_FILE    (1<<10)

/*! Maximum length of VFS path prefix (not including zero terminator)  */
#define VFS_PATH_MAX 15

/*! Maximum number of registered filesystems */
#define VFS_MAX_COUNT 5

/**
 * Maximum number of (global) file descriptors.
 */
#define VFS_MAX_FDS         64

struct kfile_desc {
	id_t id;    // kernel object id
	int vfs_fd; // vfs file descriptor id
};

#endif /* _K_VFS_C_ */
