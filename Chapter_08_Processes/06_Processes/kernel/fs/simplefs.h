/*! Simple file system */
#pragma once

#include "storage.h"

int k_simplefs_init(kstorage_t* storage, const char* mountpoint);

#define _K_SIMPLEFS_C_

#ifdef _K_SIMPLEFS_C_

#include "../thread.h"
#include <types/time.h>
#include "vfs.h"

#define MAXFILESONDISK    16
#define MAXFILENAMESIZE    16
#define MAXFILEBLOCKS    16

//file node
struct sfs_node {
	char node_name[MAXFILENAMESIZE];
	size_t id;      // position in file table
	timespec_t tc;  // time of file creation
	timespec_t ta;  // time of last access
	timespec_t tm;  // time of last modification
	size_t size;    // total size in bytes
	size_t blocks;  // allocated blocks
	size_t block[MAXFILEBLOCKS]; // where are blocks on disk
} __attribute__((packed));

//file table
struct sfs_table {
	size_t block_size; //size of one block
	size_t blocks; //number of blocks on disk
	size_t max_files; //maximum number of files
	struct sfs_node fd[MAXFILESONDISK]; //array of simplefs nodes
	char free[1]; //array of chars indicating free/occupied blocks
} __attribute__((packed));

/* disk blocks:
   0-X: [file_table] => sizeof(struct file_table)
   X+1: free block or file content
*/

//open file descriptor
struct sfs_file_desc {
	struct sfs_node* tfd; //pointer to descriptor in file table in memory
	int flags;
	size_t fp; //file pointer, offset from beginning
};

#endif /* _K_SIMPLEFS_C_ */
