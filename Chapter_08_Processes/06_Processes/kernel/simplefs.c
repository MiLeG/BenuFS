#define _K_SIMPLEFS_C_

#include <kernel/errno.h>
#include <kernel/memory.h>
#include <types/io.h>
#include <lib/string.h>
#include "simplefs.h"
#include "device.h"
#include "time.h"
#include "memory.h"
#include "vfs.h"

static kdevice_t* disk;
#define DISK_WRITE(buffer, blocks, first_block) \
k_device_send(buffer, (first_block << 16) | blocks, 0, disk);

#define DISK_READ(buffer, blocks, first_block) \
k_device_recv(buffer, (first_block << 16) | blocks, 0, disk);

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

static struct fs_table* ft;
static size_t ft_size;

//TODO - dinamična alokacija ovoga s max brojem otvorenih fileova kao argument u initu
static struct simplefs_file_desc fd_table[VFS_MAX_FDS];


static int k_simplefs_open_file(char* pathname, int flags, mode_t mode);
static int k_simplefs_close_file(int fd);
static int k_simplefs_read_write(int fd, void* buffer, size_t size, int op);

int k_simplefs_init(char* disk_device, size_t bsize, size_t blocks){
	disk = k_device_open(disk_device, 0);
	assert(disk);

	//initialize disk
	ft_size = sizeof(struct fs_table) + blocks;
	ft_size = (ft_size + bsize - 1) / bsize;
	ft = kmalloc(ft_size * bsize);
	memset(ft, 0, ft_size * bsize);
	ft->file_system_type = FS_TYPE;
	strcpy(ft->partition_name, disk_device);
	ft->block_size = bsize;
	ft->blocks = blocks;
	ft->max_files = MAXFILESONDISK;

	int i;
	for(i = ft_size; i < ft->blocks; i++)
		ft->free[i] = 1;

	DISK_WRITE(ft, ft_size, 0);

	memset(&fd_table, 0, sizeof(fd_table));

	//create vfs struct and register
	kvfs_t vfs = {
			.open = k_simplefs_open_file,
			.close = k_simplefs_close_file,
			.read_write = k_simplefs_read_write
	};
	k_vfs_register("/simplefs", &vfs);


	return 0;
}

int k_simplefs_open_file(char* pathname, int flags, mode_t mode){
	struct fs_node* tfd = NULL;
	const char* fname = pathname;

	//check if file already open
	struct simplefs_file_desc* fd = fd_table;
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
		for(i = 0; i < ft->max_files; i++){
			if(strcmp(ft->fd[i].node_name, fname) == 0){
				tfd = &ft->fd[i];
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
		for(i = 0; i < ft->max_files; i++){
			if(ft->fd[i].node_name[0] == 0){
				tfd = &ft->fd[i];
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
	fd = fd_table;
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

int k_simplefs_close_file(int fd){
	if(fd < 0 || fd >= VFS_MAX_FDS)
		return -EBADF;

	fd_table[fd].tfd = NULL;
	fd_table[fd].flags = 0;
	fd_table[fd].fp = 0;

	return 0;
}

//op = 0 => write, otherwise =>read
int k_simplefs_read_write(int fd_index, void* buffer, size_t size, int op){
	if(fd_index < 0 || fd_index >= VFS_MAX_FDS)
		return -EBADF;

	struct simplefs_file_desc* fd = &fd_table[fd_index];

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

		char buf[ft->block_size];
		size_t leftToRead = size;
		size_t block = fd->fp / ft->block_size;


		while(leftToRead > 0 &&
			  fd->fp < fd->tfd->size &&
			  fd->tfd->block[block] >= 0){

			DISK_READ(buf, 1, fd->tfd->block[block]);


			size_t toCopy = min(min(leftToRead, fd->tfd->size - fd->fp), ft->block_size - fd->fp % ft->block_size);

			memcpy(buffer, buf + fd->fp % ft->block_size, toCopy);
			buffer += toCopy;
			fd->fp += toCopy;
			leftToRead -= toCopy;
			block++;

		}

		//save time of last access
		timespec_t t;
		kclock_gettime(CLOCK_REALTIME, &t);
		fd->tfd->ta = t;
		DISK_WRITE(ft, ft_size, 0);

		return size - leftToRead;

	}else{
		//assume there is enough space on disk

		//write ...
		//if ...->block[x] == 0 => find free block on disk
		//when fp isn't block start, read block from disk first
		//and then replace fp+ bytes ... and then write block back

		//size_t block = fd->fp / ft->block_size;

		char buf[ft->block_size];
		size_t leftToWrite = size;
		size_t block = fd->fp / ft->block_size;
		size_t maxfilesize = ft->block_size * MAXFILEBLOCKS;


		while(leftToWrite > 0 && fd->fp < maxfilesize){
			if(fd->tfd->block[block] == 0){
				size_t freeBlock = -1;
				for(size_t i = 0; i < ft->blocks; ++i){
					if(ft->free[i]){
						ft->free[i] = 0;
						freeBlock = i;

						break;
					}
				}
				if(freeBlock == -1) break;
				fd->tfd->block[block] = freeBlock;
			}
			DISK_READ(buf, 1, fd->tfd->block[block]);

			size_t toCopy = min(leftToWrite, ft->block_size - fd->fp % ft->block_size);
			memcpy(buf + fd->fp % ft->block_size, buffer, toCopy);

			DISK_WRITE(buf, 1, fd->tfd->block[block]);


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

		DISK_WRITE(ft, ft_size, 0);

		return size - leftToWrite;
	}
}
