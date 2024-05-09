#include <arch/storage.h>
#include <kernel/memory.h>
#include <kernel/errno.h>
#include <lib/string.h>

static const int BlockSize = 512;
static const int Blocks = 4096;

static int init(void* params, storage_t* storage);
static int destroy(void* params, storage_t* storage);
static int get_storage_info(storage_t* storage, size_t* sector_size, size_t* sectors);
static int load_sectors(storage_t* storage, size_t first_sector, size_t sectors, void* buffer);
static int store_sectors(storage_t* storage, size_t first_sector, size_t sectors, void* buffer);

storage_t simpledisk = (storage_t)
		{
				.name =    "SIMPLEDISK",

				.init =        init,
				.destroy =    destroy,
				.get_storage_info = get_storage_info,
				.load_sectors = load_sectors,
				.store_sectors = store_sectors,

				.params =    NULL
		};


static int init(void* params, storage_t* storage){
	ASSERT_AND_RETURN_ERRNO(storage->params == NULL, -EINVAL);

	storage->params = kmalloc(BlockSize * Blocks);

	ASSERT_AND_RETURN_ERRNO(storage->params != NULL, -ENOMEM);

	memset(storage->params, 0, BlockSize * Blocks);

	return 0;
}

static int destroy(void* params, storage_t* storage){
	ASSERT_AND_RETURN_ERRNO(storage->params != NULL, -EINVAL);

	kfree(storage->params);

	return 0;
}

static int get_storage_info(storage_t* storage, size_t* sector_size, size_t* sectors){
	ASSERT_AND_RETURN_ERRNO(storage->params != NULL, -EINVAL);

	*sector_size = BlockSize;
	*sectors = Blocks;

	return 0;
}

static int load_sectors(storage_t* storage, size_t first_sector, size_t sectors, void* buffer){
	ASSERT_AND_RETURN_ERRNO(storage->params != NULL, -EINVAL);

	memcpy(buffer, storage->params + first_sector * BlockSize, sectors * BlockSize);

	return 0;
}

static int store_sectors(storage_t* storage, size_t first_sector, size_t sectors, void* buffer){
	ASSERT_AND_RETURN_ERRNO(storage->params != NULL, -EINVAL);

	memcpy(storage->params + first_sector * BlockSize, buffer, sectors * BlockSize);

	return 0;
}
