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
	HLO_FS_Page_Free  = 0x3,
	HLO_FS_Page_Used  = 0x2,
	HLO_FS_Page_Bad   = 0x1,
	HLO_FS_Page_Dirty = 0x0,
} HLU_FS_Page_State;

/**
 * hlo_fs_init - attempt to read hlo_fs layout from flash
 *
 * @returns <0 on error, 0 on success
 **/
int32_t hlo_fs_init();

/**
 * hlo_fs_format - (re)format the hlo_fs flash filesystem
 *
 * @param num_partitions - number of HLO_FS_Partition_Info elements to process
 * @param partitions - array of HLO_FS_Partition_Info elements to instantiate
 *                     NOTE: block_count element for HLO_FS_Partition_Bitmap
 *                           will be auto-calculated based on flash size and
 *                           HLO_FS_Partition_Data will be auto-calculated to
 *                           fill the flash size after other partitions have
 *                           been created.
 *
 * @returns <0 on error, 0 on success
 **/
int32_t hlo_fs_format(uint16_t num_partitions, HLO_FS_Partition_Info *partitions, bool force_format);

/**
 * hlo_fs_get_partition_info - get the offset and size of a specific partition
 *
 * @param id - partition ID to search for
 * @param pinfo - pointer to a HLO_FS_Partition_Info to receive the partition information
 *
 * @returns <0 on error, 0 when partition not found, 1 if a partition is found
 **/
int32_t hlo_fs_get_partition_info(enum HLO_FS_Partition_ID id, HLO_FS_Partition_Info *pinfo);
