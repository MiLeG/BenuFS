#define _K_VFS_C_

#include <lib/string.h>
#include <types/errno.h>
#include <kernel/memory.h>
#include "vfs.h"
#include "../memory.h"
#include ASSERT_H

typedef int8 vfs_index_t;
typedef uint8 local_fd_t;

typedef struct {
	vfs_index_t vfs_index;
	local_fd_t local_fd;
} fd_table_t;
#define FD_TABLE_ENTRY_UNUSED   (fd_table_t) {.vfs_index = -1, .local_fd = -1 }

typedef struct _vfs_entry_t_ {
	klfs_t lfs;          // contains pointers to FS functions
	char path_prefix[LFS_PREFIX_MAX]; // path prefix mapped to this VFS
//    void* ctx;              // optional pointer which can be passed to VFS
	int offset;             // index of this structure in s_vfs array
} vfs_entry_t;

static vfs_entry_t* s_vfs[VFS_MAX_COUNT] = { 0 };
static size_t s_lfs_count = 0;

static fd_table_t s_fd_table[VFS_MAX_FDS] = { [0 ... VFS_MAX_FDS - 1] = FD_TABLE_ENTRY_UNUSED};
static struct kfile_desc* last_check;

static const vfs_entry_t* get_lfs_for_path(const char* path){
	//TODO - check for similiar prefixes, pick longer prefix (e.g. /uart/ and /uart/dev0/)
	const vfs_entry_t* vfs_match = NULL;
	for(size_t i = 0; i < s_lfs_count; ++i){
		const vfs_entry_t* vfs = s_vfs[i];

		if(strncmp(path, vfs->path_prefix, strlen(vfs->path_prefix)) == 0){
			vfs_match = vfs;
			break;
		}
	}

	return vfs_match;
}

static int vfs_fd_valid(int vfs_fd){
	return (vfs_fd >= 0) && (vfs_fd < VFS_MAX_FDS);
}

static const vfs_entry_t* get_lfs_for_index(int index){
	if(index < 0 || index >= s_lfs_count){
		return NULL;
	}else{
		return s_vfs[index];
	}
}

static const vfs_entry_t* get_lfs_for_fd(int fd){
	const vfs_entry_t* vfs = NULL;
	if(vfs_fd_valid(fd)){
		const int index = s_fd_table[fd].vfs_index; // single read -> no locking is required
		vfs = get_lfs_for_index(index);
	}
	return vfs;
}

static inline int get_local_fd(const vfs_entry_t* vfs, int fd){
	int local_fd = -1;

	if(vfs && vfs_fd_valid(fd)){
		local_fd = s_fd_table[fd].local_fd; // single read -> no locking is required
	}

	return local_fd;
}


static const char* translate_path(const vfs_entry_t* vfs, const char* src_path){
	ASSERT(strncmp(src_path, vfs->path_prefix, strlen(vfs->path_prefix)) == 0);
	return src_path + strlen(vfs->path_prefix);
}

int k_lfs_register(const char* base_path, const klfs_t* lfs){
	size_t len = strlen(base_path);
	/* empty prefix is allowed, "/" is not allowed */
	if((len == 1) || (len > LFS_PREFIX_MAX)){
		return -EINVAL;
	}
	/* prefix has to start with "/" and not end with "/" */
	if(len >= 2 && ((base_path[0] != '/') || (base_path[len - 1] == '/'))){
		return -EINVAL;
	}

	vfs_entry_t* entry = (vfs_entry_t*) kmalloc(sizeof(vfs_entry_t));
	if(entry == NULL){
		return -ENOMEM;
	}
	size_t index;
	for(index = 0; index < s_lfs_count; ++index){
		if(s_vfs[index] == NULL){
			break;
		}
	}
	if(index == s_lfs_count){
		if(s_lfs_count >= VFS_MAX_COUNT){
			kfree(entry);
			return -ENOMEM;
		}
		++s_lfs_count;
	}
	s_vfs[index] = entry;
	strcpy(entry->path_prefix, base_path); // we have already verified argument length

	memcpy(&entry->lfs, lfs, sizeof(klfs_t));
//	entry->ctx = ctx;
	entry->offset = index;

	return EXIT_SUCCESS;

}

int k_lfs_unregister(const char* base_path){
	for(size_t i = 0; i < s_lfs_count; ++i){
		vfs_entry_t* vfs = s_vfs[i];
		if(vfs == NULL){
			continue;
		}
		if(strcmp(base_path, vfs->path_prefix) == 0){
			kfree(vfs);
			s_vfs[i] = NULL;

			// Delete all references from the FD lookup-table
			for(int j = 0; j < VFS_MAX_COUNT; ++j){
				if(s_fd_table[j].vfs_index == i){
					s_fd_table[j] = FD_TABLE_ENTRY_UNUSED;
				}
			}

			return EXIT_SUCCESS;
		}
	}
	return -EINVAL;
}

int k_vfs_is_file_open(descriptor_t* desc){
	kobject_t* kobj;
	kprocess_t* proc;

	proc = kthread_get_process(NULL);
	kobj = desc->ptr;

	if(kobj && list_find(&proc->kobjects, &kobj->list) &&
	   (kobj->flags & KTYPE_FILE) != 0){
		struct kfile_desc* fd = kobj->kobject;
		if(fd && fd->id == desc->id && vfs_fd_valid(fd->vfs_fd) && s_fd_table[fd->vfs_fd].vfs_index != -1){
			last_check = fd;
			return 0;
		}
	}

	return -1;
}

int k_vfs_open_file(char* pathname, int flags, mode_t mode, descriptor_t* desc){
	char* fname = pathname;

	//check for conflicting flags
	if(((flags & O_RDONLY) && (flags & O_WRONLY)) ||
	   ((flags & O_RDONLY) && (flags & O_RDWR)) ||
	   ((flags & O_WRONLY) && (flags & O_RDWR)))
		return -EINVAL;

	const vfs_entry_t* vfs = get_lfs_for_path(fname);
	if(vfs == NULL){
		return -ENOENT;
	}
	const char* path_within_lfs = translate_path(vfs, fname);
	int fd_within_lfs = vfs->lfs.open((char*) path_within_lfs, flags, mode);

	int vfs_index = -1;
	if(fd_within_lfs >= 0){
//		_lock_acquire(&s_fd_table_lock);
		for(int i = 0; i < VFS_MAX_FDS; ++i){
			if(s_fd_table[i].vfs_index == -1){
				s_fd_table[i].vfs_index = vfs->offset;
				s_fd_table[i].local_fd = fd_within_lfs;
//				_lock_release(&s_fd_table_lock);
				vfs_index = i;
				break;
			}
		}
//		_lock_release(&s_fd_table_lock);

		if(vfs_index == -1){
			vfs->lfs.close(fd_within_lfs);
			return -ENOMEM;
		}
	}else{
		return -ENFILE;
	}

	kprocess_t* proc;
	proc = kthread_get_process(NULL);
	//create kobject and a new struct kfile_desc
	kobject_t* kobj = kmalloc_kobject(proc, sizeof(struct kfile_desc));
	kobj->flags = KTYPE_FILE;
	kfile_desc_t* fd = kobj->kobject;
	fd->vfs_fd = vfs_index;
	fd->flags = flags;
	fd->id = k_new_id();

	//fill desc
	desc->id = fd->id;
	desc->ptr = kobj;

	return 0;
}

int k_vfs_close_file(descriptor_t* desc){
	int fd = last_check->vfs_fd;
	const vfs_entry_t* vfs = get_lfs_for_fd(fd);
	const int local_fd = get_local_fd(vfs, fd);
	if(vfs == NULL || local_fd < 0){
		return -EBADF;
	}
	int ret;
	ret = vfs->lfs.close(local_fd);

//	_lock_acquire(&s_fd_table_lock);
	s_fd_table[fd] = FD_TABLE_ENTRY_UNUSED;
//	_lock_release(&s_fd_table_lock);

	return ret;
}

int k_vfs_read_write(descriptor_t* desc, void* buffer, size_t size, int op){
	kfile_desc_t* fd = last_check;

	if((op && (fd->flags & O_WRONLY)) || (!op && (fd->flags & O_RDONLY)))
		return -EPERM;

	int fd_index = fd->vfs_fd;
	const vfs_entry_t* vfs = get_lfs_for_fd(fd_index);
	const int local_fd = get_local_fd(vfs, fd_index);
	if(vfs == NULL || local_fd < 0){
		return -EBADF;
	}

	int ret = vfs->lfs.read_write(local_fd, buffer, size, op);
	return ret;
}
