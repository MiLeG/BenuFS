#define _K_VFS_C_

#include <lib/string.h>
#include <types/errno.h>
#include <kernel/memory.h>
#include "vfs.h"
#include "../memory.h"
#include ASSERT_H

typedef int8 lfs_index_t; //index of registered LFS, -1 - empty/invalid
typedef int local_fd_t; //file descriptor handle for specific LFS

//struktura registriranog LFS-a
typedef struct _lfs_entry_t_ {
	klfs_t lfs;          // contains pointers to FS functions
	char path_prefix[LFS_PREFIX_MAX]; // path prefix mapped to this VFS
	void* ctx;              // optional pointer which can be passed to VFS
	int offset;             // index of this structure in s_vfs array
} lfs_entry_t;

//polje registriranih LFS-ova
static lfs_entry_t* lfs_entries[VFS_MAX_COUNT] = { 0 };
//broj registriranih LFS-ova
static size_t lfs_count = 0;

//struktura virtualnog datotečnog opisnika
typedef struct {
	lfs_index_t lfs_index; //indeks LFS-a u kojem se datoteka nalazi
	local_fd_t local_fd; //identifikator opisnika u lokalnom kontekstu pripadajućeg LFS-a
} virtual_fd_t;

//makro za prazan element u polju virtualnih opisnika
#define FD_TABLE_ENTRY_UNUSED   (virtual_fd_t) {.lfs_index = -1, .local_fd = -1 }

//polje virtualnih datotečnih opisnika
static virtual_fd_t virtual_fd_table[VFS_MAX_FDS] = { [0 ... VFS_MAX_FDS - 1] = FD_TABLE_ENTRY_UNUSED};
static struct kfile_desc* last_check;


//Used for checking if LFS requires context ptr
#define CHECK_PTR_CALL(ret, vfsEntry, func, ...) \
    if (vfsEntry->lfs.func == NULL) { \
        ret = -ENOSYS; \
    } \
    if (vfsEntry->lfs.flags & LFS_FLAG_CONTEXT_PTR) { \
        ret = vfsEntry->lfs.func ## _p(__VA_ARGS__, vfsEntry->ctx); \
    } else { \
        ret = vfsEntry->lfs.func(__VA_ARGS__);\
    }


static const lfs_entry_t* get_lfs_for_path(const char* path){
	//TODO - check for similiar prefixes, pick longer prefix (e.g. /uart/ and /uart/dev0/)
	const lfs_entry_t* vfs_match = NULL;
	for(size_t i = 0; i < lfs_count; ++i){
		const lfs_entry_t* vfs = lfs_entries[i];

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

static const lfs_entry_t* get_lfs_for_index(int index){
	if(index < 0 || index >= lfs_count){
		return NULL;
	}else{
		return lfs_entries[index];
	}
}

static const lfs_entry_t* get_lfs_for_fd(int fd){
	const lfs_entry_t* vfs = NULL;
	if(vfs_fd_valid(fd)){
		const int index = virtual_fd_table[fd].lfs_index; // single read -> no locking is required
		vfs = get_lfs_for_index(index);
	}
	return vfs;
}

static inline int get_local_fd(const lfs_entry_t* vfs, int fd){
	int local_fd = -1;

	if(vfs && vfs_fd_valid(fd)){
		local_fd = virtual_fd_table[fd].local_fd; // single read -> no locking is required
	}

	return local_fd;
}


static const char* translate_path(const lfs_entry_t* vfs, const char* src_path){
	ASSERT(strncmp(src_path, vfs->path_prefix, strlen(vfs->path_prefix)) == 0);
	return src_path + strlen(vfs->path_prefix);
}

int k_lfs_register(const char* base_path, const klfs_t* lfs, void* ctx){
	size_t len = strlen(base_path);
	/* empty prefix is allowed, "/" is not allowed */
	if((len == 1) || (len > LFS_PREFIX_MAX)){
		return -EINVAL;
	}
	/* prefix has to start with "/" and not end with "/" */
	if(len >= 2 && ((base_path[0] != '/') || (base_path[len - 1] == '/'))){
		return -EINVAL;
	}

	lfs_entry_t* entry = (lfs_entry_t*) kmalloc(sizeof(lfs_entry_t));
	if(entry == NULL){
		return -ENOMEM;
	}
	size_t index;
	for(index = 0; index < lfs_count; ++index){
		if(lfs_entries[index] == NULL){
			break;
		}
	}
	if(index == lfs_count){
		if(lfs_count >= VFS_MAX_COUNT){
			kfree(entry);
			return -ENOMEM;
		}
		++lfs_count;
	}
	lfs_entries[index] = entry;
	strcpy(entry->path_prefix, base_path); // we have already verified argument length

	memcpy(&entry->lfs, lfs, sizeof(klfs_t));
	entry->ctx = ctx;
	entry->offset = index;

	return EXIT_SUCCESS;

}

int k_lfs_unregister(const char* base_path){
	for(size_t i = 0; i < lfs_count; ++i){
		lfs_entry_t* vfs = lfs_entries[i];
		if(vfs == NULL){
			continue;
		}
		if(strcmp(base_path, vfs->path_prefix) == 0){
			kfree(vfs);
			lfs_entries[i] = NULL;

			// Delete all references from the FD lookup-table
			for(int j = 0; j < VFS_MAX_COUNT; ++j){
				if(virtual_fd_table[j].lfs_index == i){
					virtual_fd_table[j] = FD_TABLE_ENTRY_UNUSED;
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
		if(fd && fd->id == desc->id && vfs_fd_valid(fd->vfs_fd) && virtual_fd_table[fd->vfs_fd].lfs_index != -1){
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

	const lfs_entry_t* vfs = get_lfs_for_path(fname);
	if(vfs == NULL){
		return -ENOENT;
	}
	const char* path_within_lfs = translate_path(vfs, fname);
	int fd_within_lfs;
	CHECK_PTR_CALL(fd_within_lfs, vfs, open, (char*) path_within_lfs, flags, mode);
//	int fd_within_lfs = vfs->lfs.open((char*) path_within_lfs, flags, mode);

	int vfs_index = -1;
	if(fd_within_lfs >= 0){
//		_lock_acquire(&s_fd_table_lock);
		for(int i = 0; i < VFS_MAX_FDS; ++i){
			if(virtual_fd_table[i].lfs_index == -1){
				virtual_fd_table[i].lfs_index = vfs->offset;
				virtual_fd_table[i].local_fd = fd_within_lfs;
//				_lock_release(&s_fd_table_lock);
				vfs_index = i;
				break;
			}
		}
//		_lock_release(&s_fd_table_lock);

		if(vfs_index == -1){
			int ret;
			CHECK_PTR_CALL(ret, vfs, close, fd_within_lfs);
			(void) ret; //to resolve "ret set but not used" compiler warning
//			vfs->lfs.close(fd_within_lfs);
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
	const lfs_entry_t* vfs = get_lfs_for_fd(fd);
	const int local_fd = get_local_fd(vfs, fd);
	if(vfs == NULL || local_fd < 0){
		return -EBADF;
	}
	int ret;
	CHECK_PTR_CALL(ret, vfs, close, local_fd);
//	ret = vfs->lfs.close(local_fd);

//	_lock_acquire(&s_fd_table_lock);
	virtual_fd_table[fd] = FD_TABLE_ENTRY_UNUSED;
//	_lock_release(&s_fd_table_lock);

	return ret;
}

int k_vfs_read_write(descriptor_t* desc, void* buffer, size_t size, int op){
	kfile_desc_t* fd = last_check;

	if((op && (fd->flags & O_WRONLY)) || (!op && (fd->flags & O_RDONLY)))
		return -EPERM;

	int fd_index = fd->vfs_fd;
	const lfs_entry_t* vfs = get_lfs_for_fd(fd_index);
	const int local_fd = get_local_fd(vfs, fd_index);
	if(vfs == NULL || local_fd < 0){
		return -EBADF;
	}

//	int ret = vfs->lfs.read_write(local_fd, buffer, size, op);
	int ret;
	CHECK_PTR_CALL(ret, vfs, read_write, local_fd, buffer, size, op);

	return ret;
}
