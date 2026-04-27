OUTPUT_ARCH( "riscv" )
ENTRY( _start )

/*
 * PMP NAPOT region sizes - must be power of two
 * These define the maximum sizes for each PMP region
 * Note: GNU ld does not support #define, so we use explicit values
 *
 * ROM Region: 256KB (0x40000) - power of two for NAPOT
 * RAM Region: 512KB (0x80000) - power of two for NAPOT
 */

MEMORY
{
	/*
	 * PMP Region 1: Code and Read-Only Data
	 * Start: 0x80000000, Max Size: 256KB (0x40000) - power of two for NAPOT
	 *
	 * PMP Region 2: Read-Write Data  
	 * Start: 0x80100000, Max Size: 512KB (0x80000) - power of two for NAPOT
	 */
	rom (rxa) : ORIGIN = 0x80000000, LENGTH = 512K
	ram (wxa) : ORIGIN = 0x80100000, LENGTH = 1024K    /* Aligned to 1MB for PMP */
}

SECTIONS
{
	/*
	 * PMP Region 1: Code and Read-Only Data
	 * Start: 0x80000000, Size: 256KB (0x40000)
	 * NAPOT encoding requires power-of-two size and alignment
	 */
	.init 0x80000000 :
	{
		_text = .;
		KEEP (*(SORT_NONE(.init)))
	} >rom AT>rom
	
	.text :
	{
		*(.text.unlikely .text.unlikely.*)
		*(.text.startup .text.startup.*)
		*(.text .text.*)
		*(.gnu.linkonce.t.*)
	} >rom AT>rom
	
	.fini :
	{
		KEEP (*(SORT_NONE(.fini)))
		_etext = .;
	} >rom AT>rom

	.rodata.align :
	{
		. = ALIGN(8);
		_rodata = .;
	} >rom AT>rom

	.rodata.start :
	{
		_rodata_lma = LOADADDR(.rodata.start);
	} >rom AT>rom

	.rodata :
	{
		*(.rdata)
		*(.rodata .rodata.*)
		*(.gnu.linkonce.r.*)
	} >rom AT>rom

	/* Align and pad to PMP region boundary (256KB = 0x40000) */
	.rodata.pmp_pad :
	{
		. = ALIGN(256K);
		/* Ensure we don't exceed the region size */
		ASSERT(. <= 0x80000000 + 256K, "ROM region exceeds PMP NAPOT size!");
		_erodata = .;
	} >rom AT>rom

	/* 
	 * Optional privileged functions - placed after ROM region 
	 * These are NOT part of the main PMP region
	 */
	.privileged_functions 0x80040000 :
	{
		*(privileged_functions)
		FILL(0xDEAD);
	} >rom AT>rom

	.tdata : {
		*(.tdata .tdata.*)
	} >rom AT>rom

	.tbss (NOLOAD) : {
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
		PROVIDE( __tbss_end = . );
		PROVIDE( __tls_end = . );
	} >rom AT>rom
	PROVIDE( __tdata_source = LOADADDR(.tdata) );
	PROVIDE( __tdata_source_end = LOADADDR(.tdata) + SIZEOF(.tdata) );
	PROVIDE( __tdata_size = SIZEOF(.tdata) );
	PROVIDE( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );
	PROVIDE( __tbss_start = ADDR(.tbss) );
	PROVIDE( __tbss_size = SIZEOF(.tbss) );
	PROVIDE( __tls_size = __tls_end - ADDR(.tdata) );
	PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );

	/*
	 * PMP Region 2: Read-Write Data
	 * Start: 0x80100000, Size: 512KB (0x80000)
	 * NAPOT encoding requires power-of-two size and alignment
	 */
	.data.align 0x80100000 :
	{
		. = ALIGN(8);
		_data = .;
	} >ram AT>rom

	.data.start :
	{
		_data_lma = LOADADDR(.data.start);
	} >ram AT>rom

	.data :
	{
		*(.data .data.*)
		*(.gnu.linkonce.d.*)
		. = ALIGN(8);
		PROVIDE( __global_pointer$ = . + 0x800 );
		*(.sdata .sdata.*)
		*(.sdata2 .sdata2.*)
		*(.gnu.linkonce.s.*)
		. = ALIGN(8);
		*(.srodata.cst16)
		*(.srodata.cst8)
		*(.srodata.cst4)
		*(.srodata.cst2)
		*(.srodata .srodata.*)

		. = ALIGN(8);
		_edata = .;
	} >ram AT>rom

	.bss.align :
	{
		. = ALIGN(8);
		_bss = .;
	} >ram AT>rom

	.bss.start :
	{
		_bss_lma = LOADADDR(.bss.start);
	} >ram AT>rom

	.bss :
	{
		*(.sbss*)
		*(.gnu.linkonce.sb.*)
		*(.bss .bss.*)
		*(.gnu.linkonce.b.*)
		*(COMMON)
	} >ram AT>rom

	/* Align and pad to PMP region boundary (512KB = 0x80000) */
	.bss.pmp_pad :
	{
		. = ALIGN(512K);
		/* Ensure we don't exceed the region size */
		ASSERT(. <= 0x80100000 + 512K, "RAM region exceeds PMP NAPOT size!");
		_ebss = .;
	} >ram AT>rom

	. = ALIGN(8);
	_end = .;

	/*
	 * Optional privileged data - placed after main RAM region
	 * These are NOT part of the main PMP region
	 */
	.privileged_data.align 0x80180000 :
	{
		_privileged_data = .;
	} >ram AT>rom

	.privileged_data.start :
	{
		_privileged_data_lma = LOADADDR(.privileged_data.start);
	} >ram AT>rom

	.privileged_data :
	{
		*(privileged_data)
		FILL(0xDEAD);
		_eprivileged_data = .;
	} >ram AT>rom

	.stack :
	{
		. = ALIGN(16);
		. += __stack_size;
		_stack_top = .;
	} >ram AT>ram
}
