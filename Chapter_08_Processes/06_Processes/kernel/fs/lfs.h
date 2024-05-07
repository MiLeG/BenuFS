#ifndef _K_LFS_C_
#define _K_LFS_C_

/*! LFS */
typedef struct _klfs_t_ {
	int (* open)(char* pathname, int flags, mode_t mode);

	int (* close)(int fd);

	int (* read_write)(int fd, void* buffer, size_t size, int op);
} klfs_t;

int k_lfs_register(const char* base_path, const klfs_t* lfs);
int k_lfs_unregister(const char* base_path);

#endif
