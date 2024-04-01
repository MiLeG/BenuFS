/*! Simple file system */
#pragma once

int k_simplefs_init(char* disk_device, size_t bsize, size_t blocks);

#define _K_SIMPLEFS_C_

#ifdef _K_SIMPLEFS_C_

#include "thread.h"
#include <types/time.h>
#include "vfs.h"

#define FS_TYPE        42 //any number will do :)

#define MAXFILESONDISK    16
#define MAXFILENAMESIZE    16
#define MAXFILEBLOCKS    16

struct fs_node {
	char node_name[MAXFILENAMESIZE];
	size_t id;      // position in file table
	timespec_t tc;  // time of file creation
	timespec_t ta;  // time of last access
	timespec_t tm;  // time of last modification
	size_t size;    // total size in bytes
	size_t blocks;  // allocated blocks
	size_t block[MAXFILEBLOCKS]; // where are blocks on disk
} __attribute__((packed));

struct fs_table {
	int file_system_type;
	char partition_name[MAXFILENAMESIZE];
	size_t block_size;
	size_t blocks;
	size_t max_files;
	struct fs_node fd[MAXFILESONDISK];
	char free[1]; // bitmap would be better, but ...
} __attribute__((packed));

/* disk blocks:
   0-X: [file_table] => sizeof(struct file_table)
   X+1: free block or file content
*/

//open file descriptor
struct simplefs_file_desc {
	struct fs_node* tfd; //pointer to descriptor in file table in memory
	int flags;
	size_t fp; //file pointer, offset from beginning
};

#endif /* _K_SIMPLEFS_C_ */
