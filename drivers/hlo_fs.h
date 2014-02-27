// vi:sw=4:ts=4

#pragma once

#include <stdint.h>

// Design:
//    - We keep track of page states by using a bitmap and exploiting the inherent nature
//      of flash. Flash defaults to logic high upon erasure and may transition to 0 on a
//      bit-by-bit basis via a write. Exploiting this nature allows us to save on flash
//      erase cycles, thus negating the need for more complex wear leveling.
//    - Pages can be:
//        - Free  (2'b11, aka the natural state)
//        - Used  (2'b10)
//        - Bad   (2'b01, physically bad page)
//        - Dirty (2'b00 aka need erasure)
//    - It is the duty of the code to persist the Bad page state across erase cycles. Once
//      a page has been marked bad, it should never be given a second chance at being used.

// Layout:
//    - The layout of the data stored in flash is described by the configuration data
//      which is stored in block 0.
//    - The structure is versioned for compatibility.
//    - See HLO_FS_Layout_vX structure(s) for composition
//    - The Config and Bitmap partitions are always at the beggining of block storage,
//      following partitions may come in any order.

#define BLOCK_RECORD_SIZE 2

enum HLO_FS_Return_Value {
	HLO_FS_Invalid_Parameter = -1,
	HLO_FS_Not_Initialized   = -2,
	HLO_FS_Media_Error       = -3,
	HLO_FS_Already_Valid     = -4,
	HLO_FS_Not_Found         = -5,
	HLO_FS_Not_Enough_Space  = -6,
};

// Storage oriented constants and structures
enum HLO_FS_Partition_ID {
	HLO_FS_Partition_Config = 0,
	HLO_FS_Partition_Bitmap = 1,
	HLO_FS_Partition_Data   = 2,
	HLO_FS_Partition_Recovery = 3,
	HLO_FS_Partition_Update   = 4,
	HLO_FS_Partition_Crashlog = 5,
	HLO_FS_Partition_Max
};

typedef struct __attribute__((__packed__)) {
	enum HLO_FS_Partition_ID id;
	uint16_t              block_offset;
	uint16_t              block_count;
} HLO_FS_Partition_Info;

static const uint8_t HLO_FS_Layout_v1_Magic[4] = "HFv1";
typedef struct __attribute__((__packed__)) {
	uint8_t             magic[4];
	uint8_t             num_partitions;
	HLO_FS_Partition_Info  partitions[];
} HLO_FS_Layout_v1;

typedef enum {
	HLO_FS_Page_Max   = 0x4,
	HLO_FS_Page_Free  = 0x3,
	HLO_FS_Page_Used  = 0x2,
	HLO_FS_Page_Bad   = 0x1,
	HLO_FS_Page_Dirty = 0x0,
} HLO_FS_Page_State;

static const uint8_t HLO_FS_Page_State_Bits = 2;

struct HLO_FS_Bitmap_Record {
    enum HLO_FS_Partition_ID id;

    // calculated absolute start and end addresses
    // (fall within bitmap partition)
    uint32_t bitmap_start_addr;
    uint32_t bitmap_end_addr;

	// space tracking
	uint32_t page_stats[HLO_FS_Page_Max];
	uint32_t min_free_bytes;

    // for caching linear scan results
    uint32_t bitmap_read_ptr;
    uint32_t bitmap_write_ptr;
	uint8_t  bitmap_read_element;
	uint8_t  bitmap_write_element;
};

struct HLO_FS_Page_Range {
	uint32_t start_page;
	uint32_t end_page;
};

/**
 * General FileSystem calls
 **/

/**
 * hlo_fs_init - attempt to read hlo_fs layout from flash
 *
 * @returns <0 on error, 0 on success
 **/
int32_t hlo_fs_init();

/**
 * hlo_fs_format - (re)format and partition the hlo_fs flash filesystem
 *
 * @param num_partitions - number of HLO_FS_Partition_Info elements to process
 * @param partitions - array of HLO_FS_Partition_Info elements to instantiate
 *                     NOTE: block_count element for HLO_FS_Partition_Bitmap
 *                           will be auto-calculated based on flash size and
 *                           HLO_FS_Partition_Data will be auto-calculated to
 *                           fill the flash size after other partitions have
 *                           been created.
 * @param force_format - set this flag to force a re-format even if the device
 *                       currently contains a valid configuration block
 *
 * @returns <0 on error, 0 on success
 **/
int32_t hlo_fs_format(uint16_t num_partitions, HLO_FS_Partition_Info *partitions, bool force_format);

/**
 * Partition-oriented calls
 **/

/**
 * hlo_fs_get_partition_info - get the offset and size of a specific partition
 *
 * @param id - partition ID to search for
 * @param pinfo - pointer to a HLO_FS_Partition_Info to receive the partition information
 *
 * @returns <0 on error, 0 when partition is found
 **/
int32_t hlo_fs_get_partition_info(enum HLO_FS_Partition_ID id, HLO_FS_Partition_Info **pinfo);

/**
 * hlo_fs_page_count - count the number of pages allocated to a partition
 *
 * @param id - ID of the partition to check the capacity of
 *
 * @returns <0 on error, otherwise capacity in number of pages
 **/
int32_t hlo_fs_page_count(enum HLO_FS_Partition_ID id);

/**
 * hlo_fs_free_page_count - count the number of whole free pages in a partition
 *
 * @parm id - partition ID to count free pages in
 *
 * @returns <0 on error, otherwise the number of free pages
 **/
int32_t hlo_fs_free_page_count(enum HLO_FS_Partition_ID id);

/**
 * hlo_fs_reclaim - erase 'dirty' pages and mark them as free
 *
 * @param id - ID of the partition to scan for dirty pages to erase
 *
 * NOTE: this is ideally called when on the charger as it is a higher
 *       power level activity
 *
 * @returns <0 on error, otherwise the number of pages reclaimed
 **/
int32_t hlo_fs_reclaim(enum HLO_FS_Partition_ID id);

/**
 * Data-oriented calls
 **/

/**
 * hlo_fs_append - write n bytes to the end of the partition-based circular buffer
 *
 * @param id - ID of the partition to operate on
 * @param len - number of bytes to write to the circular buffer
 * @param data - pointer to a buffer that contains bytes to be written
 *
 * @returns <0 on error, otherwise the number of bytes written
 **/
int32_t hlo_fs_append(enum HLO_FS_Partition_ID id, uint32_t len, uint8_t *data);

/**
 * hlo_fs_read - read n bytes from the beginning of the partition-based circular buffer
 *
 * @param id - ID of the partition to operate on
 * @param len - a page-size multiple of data to be read
 * @param data - pointer to a buffer to receive data
 * @param range - pointer to a HLO_FS_Page_Range structure to hold the range of pages
 *                involved in this operation. Commonly passed to hlo_fs_mark_dirty.
 *
 * @returns <0 on error, otherwise the number of bytes read
 **/ 
int32_t hlo_fs_read(enum HLO_FS_Partition_ID id, uint32_t len, uint8_t *data, struct HLO_FS_Page_Range *range);

/**
 * hlo_fs_mark_dirty - mark a page range as dirty
 *
 * @param id - ID of the partition to work on
 * @param range - pointer to a HLO_FS_Page_Range struct covering the page range to
 *                mark as being dirty. Commonly the output of a hlo_fs_read call.
 *
 * @returns <0 on error, otherwise the number of pages marked as dirty
 **/
int32_t hlo_fs_mark_dirty(enum HLO_FS_Partition_ID id, struct HLO_FS_Page_Range *range);

