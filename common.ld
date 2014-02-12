/* Library configurations */

/*GROUP(libgcc.a libc.a libm.a libnosys.a)
*/
/* Linker script to place sections and symbol values.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __exidx_start
 *   __exidx_end
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 */
ENTRY(Reset_Handler)

SECTIONS
{
	.text :
	{
		KEEP(*(.Vectors))
		*(.text*)

		KEEP(*(.init))
		KEEP(*(.fini))

		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)

		/* .dtors */
 		*crtbegin.o(.dtors)
 		*crtbegin?.o(.dtors)
 		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
 		*(SORT(.dtors.*))
 		*(.dtors)

		*(.rodata*)

		KEEP(*(.eh_frame*))
	} > TEXT

        __user_data :
        {
        	__user_data_start__ = .; /* 0x3ec00 */

		__ble_bondmngr_sys_attr_start__ = .; /* 0x3ec00 */
		. += 0x200; /* 512B */
		__ble_bondmngr_sys_attr_end__ = .;

		__ble_bondmngr_bond_info_start__ = .; /* 0x3ee00 */
		. += 0x100; /* 256B */
		__ble_bondmngr_bond_info_end__ = .;

		__unused_start__ = .; /* 0x3ef00 */
		. += 0x900;
		. = ALIGN(256);
		__unused_end__ = .;

		__app_sha1_start__ = .; /* 0x3f800 */
		. += 0x14; /* 20B */
		. = ALIGN(256);
		__app_sha1_end__ = .;

		__user_data_end__ = .; /* 0x40000 */

		/* [TODO]: Figure out whether we want to keep ble_bondmngr_*. */
        } > USER_DATA

	.ARM.extab :
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > TEXT

	__exidx_start = .;
	.ARM.exidx :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > TEXT
	__exidx_end = .;

	__etext = .;

	.data : AT (__etext)
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)

		. = ALIGN(4);
		/* preinit data */
		PROVIDE (__preinit_array_start = .);
		*(.preinit_array)
		PROVIDE (__preinit_array_end = .);

		. = ALIGN(4);
		/* init data */
		PROVIDE (__init_array_start = .);
		*(SORT(.init_array.*))
		*(.init_array)
		PROVIDE (__init_array_end = .);


		. = ALIGN(4);
		/* finit data */
		PROVIDE (__fini_array_start = .);
		*(SORT(.fini_array.*))
		*(.fini_array)
		PROVIDE (__fini_array_end = .);

		. = ALIGN(4);
		/* All data end */
		__data_end__ = .;

	} > RAM

	.bss :
	{
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)

                . = ALIGN(4);
		__bss_end__ = .;
	} > RAM

	.heap :
	{
		__end__ = .;
		end = __end__;
		*(.heap*)

                . = ALIGN(4);
		__HeapLimit = .;
	} > RAM

	/* .stack_dummy section doesn't contains any symbols. It is only
	 * used for linker to calculate size of stack sections, and assign
	 * values to stack symbols later */
	.stack_dummy :
	{
		*(.stack)
	} > RAM

	/* Set stack top to end of RAM, and stack limit move down by
	 * size of stack_dummy section */
	__StackTop = ORIGIN(RAM) + LENGTH(RAM);
	__StackLimit = __StackTop - SIZEOF(.stack_dummy);
	PROVIDE(__stack = __StackTop);

	/* Check if data + heap + stack exceeds RAM limit */
	ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
}
