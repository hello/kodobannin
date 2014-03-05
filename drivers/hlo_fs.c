// vi:sw=4:ts=4

#include <drivers/spi_nor.h>
#include <drivers/hlo_fs.h>
#include <string.h>
#include <util.h>

//test band spinor serial should be C2 21 11 41 7E 01 2B C5 07 DC 9D 38 C3 53 FC 72

static HLO_FS_Layout_v1 _layout;
static HLO_FS_Partition_Info _partitions[HLO_FS_Partition_Max];
static struct HLO_FS_Bitmap_Record _bitmap_records[HLO_FS_Partition_Max];

static inline bool
_check_layout() {
	return 0 == memcmp(_layout.magic, HLO_FS_Layout_v1_Magic, sizeof(_layout.magic));
}

int32_t
hlo_fs_init() {
	int32_t ret;
	uint32_t to_read;

	// read header
	ret = spinor_read(0, sizeof(HLO_FS_Layout_v1), (uint8_t *)&_layout);
	if (ret != sizeof(HLO_FS_Layout_v1)) {
		return HLO_FS_Media_Error;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// clear bitmap records
	memset(_bitmap_records, 0xFF, sizeof(_bitmap_records));

	printf("Number of flash partitions: %d\n", _layout.num_partitions);

	// read partition information
	memset(_partitions, 0xAA, sizeof(HLO_FS_Partition_Info) * HLO_FS_Partition_Max);
	to_read = sizeof(HLO_FS_Partition_Info) * _layout.num_partitions;
	ret = spinor_read(sizeof(HLO_FS_Layout_v1), to_read, (uint8_t *)_partitions);
	if (ret != to_read) {
		memset(_layout.magic, 0, sizeof(_layout.magic));
		printf("Error reading partition record: %d\n", ret);
		return HLO_FS_Media_Error;
	}
/*
	for (i=0; i < _layout.num_partitions; i++) {
		printf("Partition #%d\n", i);
		printf("    id     0x%X\n", _partitions[i].id);
		printf("    offset 0x%X\n", _partitions[i].block_offset);
		printf("    count  0x%X\n", _partitions[i].block_count);
	}
*/	return 0;
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
		printf("WARNING: still in SPINOR secure mode\n");
		spinor_exit_secure_mode();
	}

	// Clear flash for new layout
	ret = spinor_chip_erase();
	if (ret != 0) {
		printf("Error erasing chip: 0x%X\n", ret);
		return HLO_FS_Media_Error;
	}

	// wait for erase to complete
	spinor_wait_completion();

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
			printf("Creating partition from index %d\n", i);
			printf("    id     0x%X\n", partitions[i].id);
			printf("    offset 0x%X\n", partitions[i].block_offset);
			printf("    count  0x%X\n", partitions[i].block_count);
			parts[valid_partitions].id = partitions[i].id;
			parts[valid_partitions].block_offset = blocks_used;
			parts[valid_partitions].block_count  = partitions[i].block_count;

			blocks_used += partitions[i].block_count;
			++valid_partitions;
		}
	}
	printf("total blocks (non-data): 0x%X\n", blocks_used);

	parts[valid_partitions].id = HLO_FS_Partition_Data;
	parts[valid_partitions].block_offset = blocks_used;
	parts[valid_partitions].block_count  = nor_cfg->total_blocks - blocks_used;

	++valid_partitions; // to account for data partition

	printf("Called with %d partitions\n", num_partitions);
	printf("Configured %d partitions\n", valid_partitions);

	for (i=0; i <= valid_partitions; i++) {
		printf("Partition %d\n", i);
		printf("    id     0x%X\n", partitions[i].id);
		printf("    offset 0x%X\n", partitions[i].block_offset);
		printf("    count  0x%X\n", partitions[i].block_count);
	}

	// write out Layout
	memcpy(_layout.magic, &HLO_FS_Layout_v1_Magic[0], sizeof(_layout.magic));
	_layout.num_partitions = valid_partitions;

	ret = spinor_write(0, sizeof(HLO_FS_Layout_v1), (uint8_t *)&_layout);
	if (ret != sizeof(HLO_FS_Layout_v1)) {
		printf("Layout write returned 0x%X\n", ret);
		return HLO_FS_Media_Error;
	}
	ret = spinor_write(sizeof(HLO_FS_Layout_v1), sizeof(HLO_FS_Partition_Info)*valid_partitions, (uint8_t *)parts);
	if (ret != sizeof(HLO_FS_Partition_Info)*valid_partitions) {
		printf("Partitions write returned 0x%X\n", ret);
		return HLO_FS_Media_Error;
	}

	return 0;
}

int32_t
hlo_fs_get_partition_info(enum HLO_FS_Partition_ID id, HLO_FS_Partition_Info **pinfo) {
	uint32_t i;

	if (!pinfo) {
		return HLO_FS_Invalid_Parameter;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// invalidate the output parameter's current value
	*pinfo = NULL;

	// linear search because we don't have many of these and I'm lazy
	for (i=0; i < _layout.num_partitions; i++) {
		if (_partitions[i].id == id) {
			*pinfo = &_partitions[i];
			return 0;
		}
	}

	return HLO_FS_Not_Found;
}

int32_t
hlo_fs_page_count(enum HLO_FS_Partition_ID id) {
	HLO_FS_Partition_Info *info;
	int32_t ret;

	ret = hlo_fs_get_partition_info(id, &info);
	if (ret < 0) {
		return ret;
	}

	NOR_Chip_Config *nor_cfg = spinor_get_chip_config();

	return info->block_count * nor_cfg->pages_per_block;
}

int32_t
hlo_fs_free_page_count(enum HLO_FS_Partition_ID id) {
	int32_t count = 0;

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// do a straightfoward scan of the entire bitmap
	// this can be improved later

	return count;
}

int32_t
hlo_fs_reclaim(enum HLO_FS_Partition_ID id) {
	int32_t count = 0;

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// TODO: implement reclaimation

	return count;
}

static int32_t
_bitmap_get_partition_range(enum HLO_FS_Partition_ID id, uint32_t *start_addr, uint32_t *end_addr) {
	HLO_FS_Partition_Info *pinfo;
	int32_t ret;
	uint32_t start;
	uint32_t end;
	uint32_t bitmap_base;
	uint32_t block_count;

	if (!start_addr || !end_addr) {
		return HLO_FS_Invalid_Parameter;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// get start and end blocks for partition in question
	ret = hlo_fs_get_partition_info(id, &pinfo);
	if (ret < 0 || !pinfo) {
		return HLO_FS_Not_Found;
	}

	// these are block addresses
	start = pinfo->block_offset;
	block_count = pinfo->block_count;

	// get start and end blocks for bitmap storage to account for them
	ret = hlo_fs_get_partition_info(HLO_FS_Partition_Bitmap, &pinfo);
	if (ret < 0 || !pinfo) {
		return HLO_FS_Not_Found;
	}

	// block_size * block_offset for bitmap base address
	bitmap_base = 4096 * pinfo->block_offset;

	// we don't count the layout block or the bitmap block starting offset
	// (this is block based arithmetic)
	start -= (pinfo->block_offset + pinfo->block_count);

	//DEBUG("start block is 0x", start);
	//DEBUG("end block is   0x", end);

	// convert block addresses to sub-byte bitmap addresses
	// this is valid for partitions since they always start and
	// end on block boundaries (4k)
	start *= ((4096/256)>>2);
	end = block_count * ((4096/256)>>2);

	//DEBUG("start addr is 0x", start);
	//DEBUG("end addr is   0x", end);

	//DEBUG("start page addr is 0x", start);
	//DEBUG("end page addr is   0x", end);

	start += bitmap_base;
	end   += bitmap_base;

	//DEBUG("start bitmap addr is 0x", start);
	//DEBUG("end bitmap addr is   0x", end);

	*start_addr = start;
	*end_addr = end;

	return 0;
}

#define ALL_UNUSED_PAGES 0xFFFFFFFF
#define ALL_DIRTY_PAGES  0
#define ALL_USED_PAGES   0xAAAAAAAA
#define Page_Free_Check(A, B) ((((A >> (B*2))&0x3) ^ HLO_FS_Page_Free) == 0)
#define Page_Used_Check(A, B) ((((A >> (B*2))&0x3) ^ HLO_FS_Page_Used) == 0)
#define Get_Page_State(A,B) ((A >> (B*2))&0x3)
#define Set_Page_State(A,B,C) ((A & (0xFFFFFFFF ^ (3 << (B*2)))) | (C << (B*2)))

static int8_t
_bitmap_get_used_pos(uint32_t haystack) {
	uint32_t i;

	if (haystack == ALL_UNUSED_PAGES)
		return -1;

	for (i=0; i<16; i++) {
		if (Page_Used_Check(haystack, i)) {
			return i;
		}
	}

	return -1;
}

static int8_t
_bitmap_get_free_pos(uint32_t haystack) {
	uint32_t i;

	if (haystack == ALL_USED_PAGES)
		return -1;

	for (i=0; i<16; i++) {
		if (Page_Free_Check(haystack, i)) {
			return i;
		}
	}

	return -1;
}

static int32_t
_bitmap_find_next_free(uint32_t start, uint32_t end, uint32_t *out_addr, uint8_t *out_pos) {
	uint32_t record;
	uint32_t addr;
	int8_t pos;
	int32_t ret;

	// invalidate out parameter values
	*out_addr = -1;
	*out_pos  = -1;

	// TODO: reserve one free page so that we can recover RPos and WPos on reboot

	addr = start;
	do {
		// load bitmap record
		ret = spinor_read(addr, sizeof(record), (uint8_t *)&record);
		if (ret != sizeof(record))
			return ret;

		//check for free page
		pos = _bitmap_get_free_pos(record);
		if (pos != -1) {
			*out_addr = addr;
			*out_pos  = pos;
			return 0;
		}

		// advance to the next bitmap record
		addr += 4;
	} while (addr < end);

	return HLO_FS_Not_Found;
}

static int32_t
_bitmap_calc_page_usage(enum HLO_FS_Partition_ID id) {
	int32_t ret;
	uint32_t addr;
	uint32_t record;
	uint32_t i;

	uint32_t *page_stats = _bitmap_records[id].page_stats;

	memset(page_stats, 0, sizeof(_bitmap_records[id].page_stats));

	// iterate through the bitmap and account for page usage
	for (addr = _bitmap_records[id].bitmap_start_addr; addr < _bitmap_records[id].bitmap_end_addr; addr += 4) {
		// load bitmap record
		ret = spinor_read(addr, sizeof(record), (uint8_t *)&record);
		if (ret != sizeof(record)) {
			return ret;
		}

		for (i=0; i < (sizeof(record) * 8 / HLO_FS_Page_State_Bits); i++) {
			++page_stats[Get_Page_State(record, i)];
		}
	}
	_bitmap_records[id].min_free_bytes = 256 * page_stats[HLO_FS_Page_Free];
	printf("Page states:\n");
	printf("Dirty: %d\n", page_stats[HLO_FS_Page_Dirty]);
	printf("Bad:   %d\n", page_stats[HLO_FS_Page_Bad]);
	printf("Used:  %d\n", page_stats[HLO_FS_Page_Used]);
	printf("Free:  %d\n", page_stats[HLO_FS_Page_Free]);
	printf("Min free bytes: %d\n", _bitmap_records[id].min_free_bytes);

	return 0;
}

static int32_t
_bitmap_load_partition_record(enum HLO_FS_Partition_ID id) {
	int32_t ret;
	uint32_t addr;
	uint32_t record;
	int8_t pos, pos2;
	int8_t free_pos;
	int32_t bitmap_case = 0;

	if ((_bitmap_records[id].id & 0xFF) == 0xFF) { //need this since enum is 4-byte on host for test framework
		ret = _bitmap_get_partition_range(id, &_bitmap_records[id].bitmap_start_addr, &_bitmap_records[id].bitmap_end_addr);
		if (ret < 0)
			return ret;

		_bitmap_records[id].id = id;

		printf("Start addr is 0x%X\n", _bitmap_records[id].bitmap_start_addr);
		printf("End addr is   0x%X\n", _bitmap_records[id].bitmap_end_addr);
		// we need to detect the following scenarios:
		//   +-------------------------------------+
		// 1 |xxxxxxxxxxxxxxxxxxxFFFFFFFFFFFFFFFFFF|
		//   +-------------------------------------+
		// 2 |xxxxxxxxxxxxxxxxxxxFFFFFFFFFFFFFFFFxx|
		//   +-------------------------------------+
		// 3 |FFFFFFFFFFFFFFFFxxxxxxxxxxxxxxxFFFFFF|
		//   +-------------------------------------+
		// 4 |FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF|
		//   +-------------------------------------+
		// Legend:
		//   xx - used
		//   FF - free
		//
		// Note: there should never be multiple, discontinuous data segments in a partition

		// Case 1 and 2 - Check the start address for data
		printf("Checking case 1/2...\r\n");
		addr = _bitmap_records[id].bitmap_start_addr;
		ret = spinor_read(addr, sizeof(record), (uint8_t *)&record);
		if (ret != sizeof(record)) {
			return ret;
		}
		// if the first slot in the block isn't used then we don't have a Case 1 or 2
		pos = _bitmap_get_used_pos(record);
		printf("\t\tchk(0x%08x) = 0x%08x (pos %d)\n", addr, record, pos);
		if (pos != 0)
			goto Case_3;

		// Case 2 check. Search back from the last address for free space
		addr = _bitmap_records[id].bitmap_end_addr - 4;
		do {
			ret = spinor_read(addr, sizeof(record), (uint8_t *)&record);
			if (ret != sizeof(record)) {
				return ret;
			}

			// check our used and free bits
			pos2 = _bitmap_get_used_pos(record);
			free_pos = _bitmap_get_free_pos(record);
			printf("\t\tchk(0x%08x) = 0x%08x (pos %d)\n", addr, record, pos2);
		} while (pos2 != -1 && free_pos == -1 && (addr -= 4) &&  (addr > _bitmap_records[id].bitmap_start_addr));


		// if there is no data at the end of the partition, then settle on case 1
		printf("Case 1 check: pos2 = %d, addr = 0x%x, end = 0x%x\n", pos2, addr, _bitmap_records[id].bitmap_end_addr);
		if (pos2 == -1 && addr >= (_bitmap_records[id].bitmap_end_addr - 4)) {
			// CASE 1
			// =======================================================================================
			bitmap_case = 1;
			_bitmap_records[id].bitmap_read_ptr = _bitmap_records[id].bitmap_start_addr;
			_bitmap_records[id].bitmap_read_element = 0;
		} else {
			// CASE 2
			// =======================================================================================
			bitmap_case = 2;
			_bitmap_records[id].bitmap_read_ptr = addr;
			_bitmap_records[id].bitmap_read_element = pos2;
		}

		// Find write pointer for Case 1 / 2
		// For Case 1/2 we should search forward from the start for the first 'free' page
		addr =  _bitmap_records[id].bitmap_start_addr;
		ret = _bitmap_find_next_free(_bitmap_records[id].bitmap_start_addr, _bitmap_records[id].bitmap_end_addr, &_bitmap_records[id].bitmap_write_ptr, &_bitmap_records[id].bitmap_write_element);
		goto Case_Done;

Case_3:
		// Case 3 / 4
		printf("Checking case 3/4...\r\n");
		addr = _bitmap_records[id].bitmap_start_addr;
		do {
			ret = spinor_read(addr, sizeof(record), (uint8_t *)&record);
			if (ret != sizeof(record)) {
				return ret;
			}
			pos = _bitmap_get_used_pos(record);
			printf("\t\tchk(0x%08x) = 0x%08x (pos %d)\n", addr, record, pos);
		} while(pos == -1 && (addr += 4) && addr < _bitmap_records[id].bitmap_end_addr);

		if (addr >= _bitmap_records[id].bitmap_end_addr && pos == -1) {
			// CASE 4
			// =======================================================================================
			bitmap_case = 4;
			_bitmap_records[id].bitmap_read_ptr = _bitmap_records[id].bitmap_start_addr;
			_bitmap_records[id].bitmap_write_ptr = _bitmap_records[id].bitmap_start_addr;
			_bitmap_records[id].bitmap_read_element = 0;
			_bitmap_records[id].bitmap_write_element = 0;
		} else {
			// CASE 3
			// =======================================================================================
			bitmap_case = 3;
			_bitmap_records[id].bitmap_read_ptr = addr;
			_bitmap_records[id].bitmap_read_element = pos;

			// find write pointer by searching forward from the read_ptr for the first free slot
			ret = _bitmap_find_next_free(addr, _bitmap_records[id].bitmap_end_addr, &_bitmap_records[id].bitmap_write_ptr, &_bitmap_records[id].bitmap_write_element);
			if (ret < 0) {
				return ret;
			}
		}
Case_Done:
		// calculate page usage
		ret = _bitmap_calc_page_usage(id);
		if (ret < 0) {
			return ret;
		}

		printf("Parition 0x%X (case %d):\n", id, bitmap_case);
		printf("\tBitmap start: 0x%x\n", _bitmap_records[id].bitmap_start_addr);
		printf("\tBitmap end:   0x%x\n", _bitmap_records[id].bitmap_end_addr);
		printf("\tRead pointer: 0x%x\n", _bitmap_records[id].bitmap_read_ptr);
		printf("\tRead element: 0x%x\n", _bitmap_records[id].bitmap_read_element);
		printf("\twrite pointer:0x%x\n", _bitmap_records[id].bitmap_write_ptr);
		printf("\tWrite element:0x%x\n", _bitmap_records[id].bitmap_write_element);

	} else {
		printf("Record already cached for partition 0x%02X (0x%02X)\n", id, _bitmap_records[id].id);
	}

	return 0;
}

static int32_t
_bitmap_get_partition_record(enum HLO_FS_Partition_ID id, struct HLO_FS_Bitmap_Record **record) {
	int32_t ret;

	if (!record || !*record) {
		return HLO_FS_Invalid_Parameter;
	}

	ret = _bitmap_load_partition_record(id);
	if (ret < 0) {
		return ret;
	}

	*record = &_bitmap_records[id];

	return 0;
}

static int32_t
_bitmap_update_write_state(struct HLO_FS_Bitmap_Record *bitmap, HLO_FS_Page_State state) {
	uint32_t record;
	uint32_t new_record;
	int32_t ret;

	ret = spinor_read(bitmap->bitmap_write_ptr, sizeof(record), (uint8_t *)&record);
	if (ret != sizeof(record)) {
		printf("%s: read %d instead of %d for record size\n", __func__, ret, (int)sizeof(record));
		return HLO_FS_Media_Error;
	}

	new_record = Set_Page_State(record, bitmap->bitmap_write_element, state);
	printf("transitioning state from 0x%X to 0x%X\n", record, new_record);

	// update page statistics structure
	HLO_FS_Page_State old_state = Get_Page_State(record, bitmap->bitmap_write_element);
	--bitmap->page_stats[old_state];
	++bitmap->page_stats[state];

	if (old_state == HLO_FS_Page_Free) {
		bitmap->min_free_bytes -= 256;
	}

	ret = spinor_write(bitmap->bitmap_write_ptr, sizeof(record), (uint8_t *)&new_record);
	if (ret != sizeof(record)) {
		printf("%s: record write only wrote %d instead of %d\n", __func__, ret, (int)sizeof(record));
		return HLO_FS_Media_Error;
	}
	spinor_wait_completion();

	return 0;
}

/*
// does this make sense to exist?
static int32_t
_bitmap_find_next_available_page(enum HLO_FS_Partition_ID id) {
	int32_t ret;

	// ensure we have loaded a partition record
	ret = _bitmap_load_partition_record(id);
	if (ret < 0)
		return ret;

	// we want to search from the end of the available block space
	// to ensure we operate like a ring buffer
	return 0;
}
*/

int32_t
hlo_fs_append(enum HLO_FS_Partition_ID id, uint32_t len, uint8_t *data) {
	int32_t ret;
	struct HLO_FS_Bitmap_Record *bitmap;
	HLO_FS_Partition_Info *pinfo;
	uint32_t write_addr;
	uint32_t bytes_written;
	uint8_t verify_buffer[256];

	if (!data) {
		return HLO_FS_Invalid_Parameter;
	}

	if (len == 0) {
		return 0;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	// get the partition information
	ret = hlo_fs_get_partition_info(id, &pinfo);
	if (ret < 0) {
		return ret;
	}

	// get the partition's bitmap information
	ret = _bitmap_get_partition_record(id, &bitmap);
	if (ret < 0) {
		return ret;
	}

	// make sure we have enough room for the data
	if (len > bitmap->min_free_bytes) {
		printf("Not enough space to write %d bytes. (%d available)\n", len, bitmap->min_free_bytes);
		return HLO_FS_Not_Enough_Space;
	}

	// TODO: (long term) re-visit how this is implemented, block-by-block is simplest and safest for now
	bytes_written = 0;

	// start of loop
	while (bytes_written < len) {
		uint32_t to_write;

		to_write = len - bytes_written;
		if (to_write > 256) {
			to_write = 256;
		}

		// calculate the absolute storage offset based on bitmap information
		write_addr  = pinfo->block_offset * 4096;
		printf("write_addr base is 0x%x\n", write_addr);
		write_addr += 4096 * (bitmap->bitmap_write_ptr - bitmap->bitmap_start_addr);
		printf("write_addr after bitmap element offset of 0x%x: 0x%x\n", (bitmap->bitmap_write_ptr - bitmap->bitmap_start_addr), write_addr);
		write_addr += 256 * bitmap->bitmap_write_element;
		printf("final write_addr 0x%x\n", write_addr);

		//TODO check to make sure we're not trying to write to a 'BAD' page
		//     XXX: this should probably be done in the bitmap advancement code

		// write at most a page of data
		ret = spinor_write_page(write_addr, to_write, &data[bytes_written]);
		if (ret < 0) {
			return ret;
		}
		if (ret != to_write) {
			printf("Write returned %d instead of %d\n", ret, to_write);
			return HLO_FS_Media_Error;
		}

		// wait for the write to complete
		spinor_wait_completion();

		// TODO: should check RDSR value for error condition here

		// verify the write
		ret = spinor_read(write_addr, to_write, verify_buffer);
		if (ret != to_write) {
			printf("Write verification only read %d bytes instead of %d\n", ret, to_write);
			return HLO_FS_Media_Error;
		}
		if (memcmp(&data[bytes_written], verify_buffer, to_write) != 0) {
			printf("Write verification failed!\n");
			return HLO_FS_Media_Error;
		}

		bytes_written += ret;

		// update the bitmap
		ret = _bitmap_update_write_state(bitmap, HLO_FS_Page_Used);
		if (ret != 0) {
			printf("failed to update bitmap write state (%d)\n", ret);
			return HLO_FS_Media_Error;
		}

		//TODO advance the bitmap metadata structure
		ret = _bitmap_find_next_free(bitmap->bitmap_write_ptr, bitmap->bitmap_end_addr, &bitmap->bitmap_write_ptr, &bitmap->bitmap_write_element);
		if (ret == HLO_FS_Not_Found) {
			// search from the beginning in case of wrap-around
			ret =  _bitmap_find_next_free(bitmap->bitmap_start_addr, bitmap->bitmap_end_addr, &bitmap->bitmap_write_ptr, &bitmap->bitmap_write_element);
			if (ret !=0) {
				printf("failed to find the a free page (%d) - STORAGE IS FULL FOR PARTITION 0x%x\n", ret, id);
				return ret;
			}
		}
		printf("New write ptr and element are 0x%x and 0x%x\n", bitmap->bitmap_write_ptr, bitmap->bitmap_write_element);
		printf("Page states:\n");
		printf("Dirty: %d\n", bitmap->page_stats[HLO_FS_Page_Dirty]);
		printf("Bad:   %d\n", bitmap->page_stats[HLO_FS_Page_Bad]);
		printf("Used:  %d\n", bitmap->page_stats[HLO_FS_Page_Used]);
		printf("Free:  %d\n", bitmap->page_stats[HLO_FS_Page_Free]);
		printf("Min free bytes: %d\n", bitmap->min_free_bytes);

	}

	return ret;
}

int32_t
hlo_fs_read(enum HLO_FS_Partition_ID id, uint32_t len, uint8_t *data, struct HLO_FS_Page_Range *range) {
	int32_t count = 0;

	if (!data || !range) {
		return HLO_FS_Invalid_Parameter;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	//TODO validate range

	//TODO read pages in range into output buffer

	return count;
}

int32_t
hlo_fs_mark_dirty(enum HLO_FS_Partition_ID id, struct HLO_FS_Page_Range *range) {
	int32_t count = 0;

	if (!range) {
		return HLO_FS_Invalid_Parameter;
	}

	if (!_check_layout()) {
		return HLO_FS_Not_Initialized;
	}

	//TODO validate range

	//TODO update bitmap with 'dirty' status for each page

	return count;
}
