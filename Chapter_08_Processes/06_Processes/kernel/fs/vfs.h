/*! Virtual file system */
#pragma once

#include "lfs.h"
//Checks if given (file) descriptor is open
int k_vfs_is_file_open(descriptor_t* desc);

/**
 * Opens file by pathname
 * @param flags Bitwise OR combination of  O_CREAT, O_RDONLY, O_WRONLY, etc...
 * @param desc returned descriptor
 */
int k_vfs_open_file(char* pathname, int flags, mode_t mode, descriptor_t* desc);

//Closes given file descriptor
int k_vfs_close_file(descriptor_t* desc);

/**
 * Performs read/write operation with given file descriptor
 * @param buffer data
 * @param size size of data in bytes
 * @param op 1 - read, 0 - write
 */
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

//veza između k_obj strukture i VFS opisnika
typedef struct kfile_desc {
	id_t id;    // kernel object id
	int vfs_fd; // global file descriptor index
	int flags;
} kfile_desc_t;

#endif /* _K_VFS_C_ */
