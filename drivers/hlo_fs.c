// vi:sw=4:ts=4

#include <drivers/spi_nor.h>
#include <drivers/hlo_fs.h>
#include <app_error.h>
#include <string.h>
#include <util.h>

//test band spinor serial should be C2 21 11 41 7E 01 2B C5 07 DC 9D 38 C3 53 FC 72

static HLO_FS_Layout_v1 _layout;
static HLO_FS_Partition_Info _partitions[HLO_FS_Partition_Max];

static inline bool
_check_layout() {
	return 0 == memcmp(_layout.magic, HLO_FS_Layout_v1_Magic, sizeof(_layout.magic));
}

int32_t
hlo_fs_init() {
	int32_t ret;
	uint32_t i;
	uint32_t to_read;

	// read header
	ret = spinor_read(0, sizeof(HLO_FS_Layout_v1), (uint8_t *)&_layout);
	if (ret != sizeof(HLO_FS_Layout_v1)) {
		return HLO_FS_Media_Error;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	DEBUG("Number of flash partitions:", _layout.num_partitions);

	// read partition information
	memset(_partitions, 0xAA, sizeof(HLO_FS_Partition_Info) * HLO_FS_Partition_Max);
	to_read = sizeof(HLO_FS_Partition_Info) * _layout.num_partitions;
	ret = spinor_read(sizeof(HLO_FS_Layout_v1), to_read, (uint8_t *)_partitions);
	if (ret != to_read) {
		memset(_layout.magic, 0, sizeof(_layout.magic));
		DEBUG("Error reading partition record: ", ret);
		return HLO_FS_Media_Error;
	}

	for (i=0; i < _layout.num_partitions; i++) {
		DEBUG("Partition ", i);
		DEBUG("    id     ", _partitions[i].id);
		DEBUG("    offset ", _partitions[i].block_offset);
		DEBUG("    count  ", _partitions[i].block_count);
	}
	return 0;
}

int32_t
hlo_fs_format(uint16_t num_partitions, HLO_FS_Partition_Info *partitions, bool force_format) {
	int32_t ret;
	uint32_t total_pages = 0;
	uint32_t blocks_used = 0;
	uint32_t bitmap_blocks = 0;
	uint32_t valid_partitions = 0;
	uint32_t i;
	HLO_FS_Partition_Info parts[HLO_FS_Partition_Max];

	if (!partitions) {
		return HLO_FS_Invalid_Parameter;
	}

	// return an error if we already have a valid layout and we haven't been told to nuke it
	if (_check_layout() && !force_format) {
		return HLO_FS_Already_Valid;
	}

	// make sure we're not in secure spinor access mode
	if (spinor_in_secure_mode()) {
		PRINTS("WARNING: still in SPINOR secure mode");
		spinor_exit_secure_mode();
	}

	// Clear flash page 0 for new layout
	ret = spinor_block_erase(0);
	if (ret != 0) {
		DEBUG("Error erasing block 0 for layout: ", ret);
		return HLO_FS_Media_Error;
	}

	// calculate number of blocks needed for bitmap partition
	NOR_Chip_Config *nor_cfg = spinor_get_chip_config();
	total_pages = nor_cfg->total_blocks * nor_cfg->pages_per_block;
	bitmap_blocks = total_pages / (sizeof(uint8_t)*8/BLOCK_RECORD_SIZE) + nor_cfg->pages_per_block;
	bitmap_blocks /= nor_cfg->block_size;
	//DEBUG("bitmap would take n blocks: 0x", bitmap_blocks);

	// reserve block 0 for the layout
	blocks_used = 1;

	parts[valid_partitions].id = HLO_FS_Partition_Bitmap;
	parts[valid_partitions].block_offset = blocks_used;
	parts[valid_partitions].block_count  = bitmap_blocks;

	blocks_used += bitmap_blocks;

	// increate valid_partitions to account for Bitmap partition
	++valid_partitions;

	// iterate through partitions, fixing up their offsets in order we were called
	for (i=0; i < num_partitions; i++) {
		if (partitions[i].id != HLO_FS_Partition_Data && partitions[i].id != HLO_FS_Partition_Bitmap) {
			DEBUG("Creating partition from index ", i);
			DEBUG("    id     ", partitions[i].id);
			DEBUG("    offset ", partitions[i].block_offset);
			DEBUG("    count  ", partitions[i].block_count);
			parts[valid_partitions].id = partitions[i].id;
			parts[valid_partitions].block_offset = blocks_used;
			parts[valid_partitions].block_count  = partitions[i].block_count;

			blocks_used += partitions[i].block_count;
			++valid_partitions;
		}
	}
	DEBUG("total blocks (non-data): 0x", blocks_used);

	parts[valid_partitions].id = HLO_FS_Partition_Data;
	parts[valid_partitions].block_offset = blocks_used;
	parts[valid_partitions].block_count  = nor_cfg->total_blocks - blocks_used;

	++valid_partitions; // to account for data partition

	DEBUG("Called with n partitions: 0x", num_partitions);
	DEBUG("Configured n partitions: 0x", valid_partitions);

	for (i=0; i <= valid_partitions; i++) {
		DEBUG("Partition ", i);
		DEBUG("    id     ", parts[i].id);
		DEBUG("    offset ", parts[i].block_offset);
		DEBUG("    count  ", parts[i].block_count);
	}

	// write out Layout
	memcpy(_layout.magic, &HLO_FS_Layout_v1_Magic[0], sizeof(_layout.magic));
	_layout.num_partitions = valid_partitions;

	ret = spinor_write(0, sizeof(HLO_FS_Layout_v1), (uint8_t *)&_layout);
	if (ret != sizeof(HLO_FS_Layout_v1)) {
		DEBUG("Layout write returned 0x", ret);
		return HLO_FS_Media_Error;
	}
	ret = spinor_write(sizeof(HLO_FS_Layout_v1), sizeof(HLO_FS_Partition_Info)*valid_partitions, (uint8_t *)parts);
	if (ret != sizeof(HLO_FS_Partition_Info)*valid_partitions) {
		DEBUG("Partitions write returned 0x", ret);
		return HLO_FS_Media_Error;
	}

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
