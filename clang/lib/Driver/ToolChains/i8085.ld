/* Default linker script for i8085 bare-metal targets. */
OUTPUT_FORMAT(elf32-i8085)
ENTRY(_start)

SECTIONS
{
  . = 0x0000;

  .text : {
    *(.text .text.*)
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    *(.data .data.*)
  }

  .bss (NOLOAD) : {
    *(.bss .bss.*)
    *(COMMON)
  }
}
