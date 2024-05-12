#ifndef _K_LFS_C_
#define _K_LFS_C_

/**
 * Default value of flags member in klfs_t.
 */
#define LFS_FLAG_DEFAULT        0

/**
 * Flag which indicates that LFS needs extra context pointer in syscalls.
 */
#define LFS_FLAG_CONTEXT_PTR    1

/*! LFS
 *
 *  If flags variable is set to LFS_FLAG_CONTEXT_PTR then implement functions with _p suffix.
 * Otherwise implement functions without _p suffix.
 * */
typedef struct _klfs_t_ {
	int flags; //Extra context ptr usage.

	union {
		int (* open)(char* pathname, int flags, mode_t mode);
		int (* open_p)(char* pathname, int flags, mode_t mode, void* ctx);
	};

	union {
		int (* close)(int fd);
		int (* close_p)(int fd, void* ctx);
	};

	union {
		int (* read_write)(int fd, void* buffer, size_t size, int op);
		int (* read_write_p)(int fd, void* buffer, size_t size, int op, void* ctx);
	};
} klfs_t;

int k_lfs_register(const char* base_path, const klfs_t* lfs, void* ctx);
int k_lfs_unregister(const char* base_path);

#endif
