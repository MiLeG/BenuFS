#pragma once

#ifndef _STORAGE_C_

#include <arch/storage.h>
#include <lib/list.h>

typedef struct _kstorage_t_ {
	storage_t storage;
	id_t id;
	list_h list;
} kstorage_t;


int k_storage_init();
kstorage_t* k_storage_add(storage_t* storage);
int k_storage_remove(kstorage_t* storage);

int k_get_storage_info(size_t* sector_size, size_t* sectors, kstorage_t* kstorage);
int k_load_sectors(kstorage_t* kstorage, size_t first_sector, size_t sectors, void* buffer);
int k_store_sectors(kstorage_t* kstorage, size_t first_sector, size_t sectors, void* buffer);


#endif /* _STORAGE_C_ */

