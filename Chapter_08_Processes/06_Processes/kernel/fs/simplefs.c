#define _K_SIMPLEFS_C_

#include <kernel/errno.h>
#include <kernel/memory.h>
#include <types/io.h>
#include <lib/string.h>
#include "simplefs.h"
#include "../device.h"
#include "../time.h"
#include "../memory.h"
#include "vfs.h"

typedef struct SFSContext {
	kstorage_t* disk; //disk na kojem je inicijaliziran SFS
	struct sfs_table* ft; //file table
	size_t ft_size; //file table size

	//TODO - dinamična alokacija ovoga s max brojem otvorenih fileova kao argument u initu
	struct sfs_file_desc fd_table[VFS_MAX_FDS]; //polje otvorenih datoteka
} SFSContext;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

static int k_simplefs_open_file(char* pathname, int flags, mode_t mode, void* ctx);
static int k_simplefs_close_file(int fd, void* ctx);
static int k_simplefs_read_write(int fd, void* buffer, size_t size, int op, void* ctx);

int k_simplefs_init(kstorage_t* storage, const char* mountpoint){
	SFSContext* context = kmalloc(sizeof(SFSContext));
	context->disk = storage;
	assert(context->disk);

	size_t blocks;
	size_t bsize;
	k_get_storage_info(&bsize, &blocks, context->disk);

	//initialize disk
	context->ft_size = sizeof(struct sfs_table) + blocks;
	context->ft_size = (context->ft_size + bsize - 1) / bsize;
	context->ft = kmalloc(context->ft_size * bsize);
	memset(context->ft, 0, context->ft_size * bsize);
	context->ft->block_size = bsize;
	context->ft->blocks = blocks;
	context->ft->max_files = MAXFILESONDISK;

	int i;
	for(i = context->ft_size; i < context->ft->blocks; i++)
		context->ft->free[i] = 1;

	k_store_sectors(context->disk, 0, context->ft_size, context->ft);

	memset(&context->fd_table, 0, sizeof(context->fd_table));

	//create lfs struct and register
	klfs_t descriptor = {
			.flags = LFS_FLAG_CONTEXT_PTR,
			.open_p = k_simplefs_open_file,
			.close_p = k_simplefs_close_file,
			.read_write_p = k_simplefs_read_write
	};
	k_lfs_register(mountpoint, &descriptor, context);


	return 0;
}

int k_simplefs_open_file(char* pathname, int flags, mode_t mode, void* ctx){
	SFSContext* context = (SFSContext*) ctx;
	struct sfs_node* tfd = NULL;
	const char* fname = pathname;

	//check if file already open
	struct sfs_file_desc* fd = context->fd_table;
	for(int i = 0; i < VFS_MAX_FDS; i++){
		if(fd->tfd && strcmp(fd->tfd->node_name, fname) == 0){
			//already open!
			//if its open for reading and O_RDONLY is set in flags
			if((fd->flags & O_RDONLY) == (flags & O_RDONLY))
				tfd = fd->tfd;//fine, save pointer to descriptor
			else
				return -EADDRINUSE; //not fine

		}
		fd++;
	}

	if(!tfd){
		//not already open; check if such file exists in file table
		int i;
		for(i = 0; i < context->ft->max_files; i++){
			if(strcmp(context->ft->fd[i].node_name, fname) == 0){
				tfd = &context->ft->fd[i];
				break;
			}
		}
	}

	if(!tfd){
		//file doesn't exitst
		if((flags & (O_CREAT | O_WRONLY)) == 0)
			return -EINVAL;

		//create fs_node in fs_table
		//1. find unused descriptor
		int i;
		for(i = 0; i < context->ft->max_files; i++){
			if(context->ft->fd[i].node_name[0] == 0){
				tfd = &context->ft->fd[i];
				break;
			}
		}


		if(!tfd)
			return -ENFILE;

		//2. initialize descriptor
		strcpy(tfd->node_name, fname);
		tfd->id = i;
		timespec_t t;
		kclock_gettime(CLOCK_REALTIME, &t);
		tfd->tc = tfd->ta = tfd->tm = t;
		tfd->size = 0;
		tfd->blocks = 0;
	}

	//find free file descriptor
	fd = context->fd_table;
	for(int i = 0; i < VFS_MAX_FDS; i++){
		if(fd->tfd == NULL){
			fd->tfd = tfd;
			fd->flags = flags;
			fd->fp = 0;
			return i;
		}
		fd++;
	}

	return -ENFILE;
}

int k_simplefs_close_file(int fd, void* ctx){
	SFSContext* context = (SFSContext*) ctx;

	if(fd < 0 || fd >= VFS_MAX_FDS)
		return -EBADF;

	context->fd_table[fd].tfd = NULL;
	context->fd_table[fd].flags = 0;
	context->fd_table[fd].fp = 0;

	return 0;
}

//op = 0 => write, otherwise =>read
int k_simplefs_read_write(int fd_index, void* buffer, size_t size, int op, void* ctx){
	if(fd_index < 0 || fd_index >= VFS_MAX_FDS)
		return -EBADF;

	SFSContext* context = (SFSContext*) ctx;
	struct sfs_file_desc* fd = &context->fd_table[fd_index];

	if(op){
		//read from offset "fd->fp" to "buffer" "size" bytes

		// possible scenarios: (#=block boundary)
		// fp % block_size == 0
		// #|fp|-----------#-----------#
		//     | size |

		// fp % block_size == 0
		// #|fp|-----------#-----------#
		//     |      size     |

		// fp % block_size > 0
		// #----|fp|-------#-----------#
		//         |size|

		// fp % block_size > 0
		// #----|fp|-------#-----------#
		//         |    size    |

		char buf[context->ft->block_size];
		size_t leftToRead = size;
		size_t block = fd->fp / context->ft->block_size;


		while(leftToRead > 0 &&
			  fd->fp < fd->tfd->size &&
			  fd->tfd->block[block] >= 0){

			k_load_sectors(context->disk, fd->tfd->block[block], 1, buf);


			size_t toCopy = min(min(leftToRead, fd->tfd->size - fd->fp), context->ft->block_size - fd->fp % context->ft->block_size);

			memcpy(buffer, buf + fd->fp % context->ft->block_size, toCopy);
			buffer += toCopy;
			fd->fp += toCopy;
			leftToRead -= toCopy;
			block++;

		}

		//save time of last access
		timespec_t t;
		kclock_gettime(CLOCK_REALTIME, &t);
		fd->tfd->ta = t;
		k_store_sectors(context->disk, 0, context->ft_size, context->ft);

		return size - leftToRead;

	}else{
		//assume there is enough space on disk

		//write ...
		//if ...->block[x] == 0 => find free block on disk
		//when fp isn't block start, read block from disk first
		//and then replace fp+ bytes ... and then write block back

		//size_t block = fd->fp / ft->block_size;

		char buf[context->ft->block_size];
		size_t leftToWrite = size;
		size_t block = fd->fp / context->ft->block_size;
		size_t maxfilesize = context->ft->block_size * MAXFILEBLOCKS;


		while(leftToWrite > 0 && fd->fp < maxfilesize){
			if(fd->tfd->block[block] == 0){
				size_t freeBlock = -1;
				for(size_t i = 0; i < context->ft->blocks; ++i){
					if(context->ft->free[i]){
						context->ft->free[i] = 0;
						freeBlock = i;

						break;
					}
				}
				if(freeBlock == -1) break;
				fd->tfd->block[block] = freeBlock;
			}
			k_load_sectors(context->disk, fd->tfd->block[block], 1, buf);

			size_t toCopy = min(leftToWrite, context->ft->block_size - fd->fp % context->ft->block_size);
			memcpy(buf + fd->fp % context->ft->block_size, buffer, toCopy);

			k_store_sectors(context->disk, fd->tfd->block[block], 1, buf);


			buffer += toCopy;
			fd->fp += toCopy;
			leftToWrite -= toCopy;
			block++;
		}

		fd->tfd->size = max(fd->fp, fd->tfd->size);

		//save time of last access, modification
		timespec_t t;
		kclock_gettime(CLOCK_REALTIME, &t);
		fd->tfd->ta = fd->tfd->tm = t;

		k_store_sectors(context->disk, 0, context->ft_size, context->ft);

		return size - leftToWrite;
	}
}
