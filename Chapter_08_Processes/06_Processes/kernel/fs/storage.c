#define _K_STORAGE_C_

#include "storage.h"
#include <lib/list.h>
#include "../memory.h"
#include <kernel/errno.h>

static list_t storageDevices;


int k_storage_init(){
	list_init(&storageDevices);

	return 0;
}

kstorage_t* k_storage_add(storage_t* storage){
	ASSERT(storage);
	storage->init(NULL, storage);

	kstorage_t* stor = kmalloc(sizeof(kstorage_t));

	ASSERT(stor);

	stor->storage = *storage;
	stor->id = k_new_id();

	list_append(&storageDevices, stor, &stor->list);

	return stor;
}

int k_storage_remove(kstorage_t* kstorage){
	if(kstorage->storage.destroy)
		kstorage->storage.destroy(kstorage->storage.params, &kstorage->storage);

	list_remove(&storageDevices, 0, &kstorage->list);
	k_free_id(kstorage->id);

	kfree(kstorage);

	return 0;
}

int k_get_storage_info(size_t* sector_size, size_t* sectors, kstorage_t* kstorage){
	if(kstorage->storage.get_storage_info)
		return kstorage->storage.get_storage_info(&kstorage->storage, sector_size, sectors);
	else
		return -ENOSYS;
}

int k_load_sectors(kstorage_t* kstorage, size_t first_sector, size_t sectors, void* buffer){
	if(kstorage->storage.load_sectors)
		return kstorage->storage.load_sectors(&kstorage->storage, first_sector, sectors, buffer);
	else
		return -ENOSYS;
}

int k_store_sectors(kstorage_t* kstorage, size_t first_sector, size_t sectors, void* buffer){
	if(kstorage->storage.store_sectors)
		return kstorage->storage.store_sectors(&kstorage->storage, first_sector, sectors, buffer);
	else
		return -ENOSYS;
}

