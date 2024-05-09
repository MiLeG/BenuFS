/*! Storage - common interface  for storage drivers */
#pragma once

#include <types/io.h>

struct _storage_t_;
typedef struct _storage_t_ storage_t;

struct _storage_t_ {
	char name[DEV_NAME_LEN];

	/* device interface */
	int (* init)(void* params, storage_t* storage);
	int (* destroy)(void* params, storage_t* storage);
	int (* get_storage_info)(storage_t* storage, size_t* sector_size, size_t* sectors);
	int (* load_sectors)(storage_t* storage, size_t first_sector, size_t sectors, void* buffer);
	int (* store_sectors)(storage_t* storage, size_t first_sector, size_t sectors, void* buffer);

	void* params; //context params
};