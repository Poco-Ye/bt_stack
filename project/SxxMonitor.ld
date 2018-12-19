
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(main_code)

MEMORY
{
	sram (W!RX) : ORIGIN = 0x181000, LENGTH = 0xF0000
	DDR (W!RX) :  ORIGIN = 0x20000000, LENGTH = 0x800000
}

SECTIONS
{
	.text	:
	{
		Image__text__Base = .;
		*(.MAIN);
		*(.text)
		. = ALIGN(32);
		*(.text*)
		. = ALIGN(4);
		*(.glue_7*)
		*(.rodata)
		*(.rodata.str1.4)

		. = ALIGN(4);
		Image__text__Limit = .;
	}>DDR

	.data : 	
	{		
		. = ALIGN(4);
		Image__data__Base = .;
		*(.data)
		. = ALIGN(32);
		kernel_sym_start = .;
		*(__ksymtab)
		kernel_sym_end = .;
		. = ALIGN(4);
		Image__data__End = .;
	}>DDR    

	.LowPower : AT(Image__data__End)
	{
		. = ALIGN(4);
		sram_sector_start = .;
		*(.LowPower)
		. = ALIGN(4);
		sram_sector_end = .;
	}>sram

	.bss (NOLOAD):
	{	
		Image__bss__ZI__Base = .;
		_bss_start = .;
		*(.bss)
		*(COMMON) 

		. = ALIGN(4);
		*(Heap)
		. = ALIGN(4);

		_bss_end = .;
		__end__ = . ; 
		Image__bss__ZI__Limit = .;
		end = .;
	}>DDR 
}

