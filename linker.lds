/* entry point of the program, first instruction executed */
ENTRY(Entry) 

SECTIONS
{
	. = 0x402f0400;
		
	. = ALIGN(32);
	.text : { 
		*(.startup)
        *(.rodata)
		*(.text*)        
	}
	
	/* non-zero global and static variables initialized by the application code */
	. = ALIGN(32);
	.data : { *(.data) }
	
	/* global and static variable that are not initialized by the application code. */
	/* the compiler (hopefully) initialize this data to 0. */
	. = ALIGN(32);
	_bss_start = .;
	.bss :  { *(.bss) }
	 
	/* Put the stack inside the BSS to simplify clearing */
	. = ALIGN(32);
	.stack :
	{
		_stack_end = .;  /* the danger zone! */
		. = . + 0x10000; /* 64kB of stack memory */
		_stack_top = .;  /* stack grows upwards */
	}

	. = ALIGN(32);
	_bss_end = .;
}
