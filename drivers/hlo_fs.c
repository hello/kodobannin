// vi:sw=4:ts=4

#include <drivers/spi_nor.h>
#include <drivers/hlo_fs.h>
#include <app_error.h>
#include <string.h>
#include <util.h>

static HLO_FS_Layout_v1 _layout;

static inline bool
_check_layout() {
	return 0 == memcmp(_layout.magic, HLO_FS_Layout_v1_Magic, sizeof(_layout.magic));
}

int32_t
hlo_fs_init() {
	int32_t ret;

	ret = spinor_read(0, sizeof(HLO_FS_Layout_v1), (uint8_t *)&_layout);
	if (ret != sizeof(HLO_FS_Layout_v1)) {
		return HLO_FS_Media_Error;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	DEBUG("Number of flash partitions:", _layout.num_partitions);
	return 0;
}

int32_t
hlo_fs_format(uint16_t num_partitions, HLO_FS_Partition_Info *partitions, bool force_format) {
	int32_t ret;
	uint32_t total_pages = 0;
	uint32_t blocks_used = 0;
	uint32_t bitmap_blocks = 0;
	uint32_t i;

	if (!partitions) {
		return HLO_FS_Invalid_Parameter;
	}

	// return an error if we already have a valid layout and we haven't been told to nuke it
	if (_check_layout() && !force_format) {
		return HLO_FS_Already_Valid;
	}

	// calculate number of blocks needed for bitmap partition
	NOR_Chip_Config *nor_cfg = spinor_get_chip_config();
	total_pages = nor_cfg->total_blocks * nor_cfg->pages_per_block;
	bitmap_blocks = total_pages / (sizeof(uint8_t)*8/BLOCK_RECORD_SIZE) + nor_cfg->pages_per_block;
	bitmap_blocks /= nor_cfg->block_size;
	DEBUG("bitmap would take n blocks: 0x", bitmap_blocks);

	// calculate total non-data/config blocks
	for (i=0; i < num_partitions; i++) {
		if (partitions[i].id != HLO_FS_Partition_Data &&
			partitions[i].id != HLO_FS_Partition_Bitmap &&
			partitions[i].id != HLO_FS_Partition_Config)
			blocks_used += partitions[i].block_count;
	}
	DEBUG("total blocks (non-data): 0x", blocks_used);
	
	i = 1 + bitmap_blocks + blocks_used;
	PRINTS("\r\n");
	DEBUG("This format would make use of n blocks: 0x", i);
	DEBUG("Total blocks available: 0x", nor_cfg->total_blocks);

	return 0;
}

int32_t
hlo_fs_get_partition_info(enum HLO_FS_Partition_ID id, HLO_FS_Partition_Info *pinfo) {
	uint32_t i;
	int32_t ret;
	bool found = false;
	HLO_FS_Partition_Info record;

	if (!pinfo) {
		return HLO_FS_Invalid_Parameter;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// linear search because we don't have many of these and I'm lazy
	for (i=0; i < _layout.num_partitions; i++) {
		ret = spinor_read(0, sizeof(HLO_FS_Partition_Info), (uint8_t *)&record);
		if (ret < 0)
			return HLO_FS_Media_Error;
		if (record.id == id) {
			found = true;
			break;
		}
	}

	if (found) {
		memcpy(pinfo, &record, sizeof(record));
		return 1;
	}

	return 0;
}
