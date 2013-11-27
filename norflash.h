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
//    - See NOR_Layout_vX structure(s) for composition

// Storage oriented constants and structures
enum NOR_Section_ID {
	NOR_Section_Config = 0,
	NOR_Section_Bitmap,
	NOR_Section_Data,
	NOR_Section_Recovery,
	NOR_Section_Update,
	NOR_Section_Crashlog,
	NOR_Section_Max
};

typedef struct {
	enum NOR_Section_ID id;
	uint16_t            block_offset;
	uint16_t            block_count;
} NOR_Section;

typedef struct {
	uint8_t      version;
	uint8_t      num_sections;
	NOR_Section  sections[];
} NOR_Layout_v1;

typedef enum {
	NOR_Page_Free  = 0x3,
	NOR_Page_Used  = 0x2,
	NOR_Page_Bad   = 0x1,
	NOR_Page_Dirty = 0x0,
} NOR_Page_State;
