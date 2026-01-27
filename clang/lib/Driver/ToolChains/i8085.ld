/* Default linker script for i8085 bare-metal targets. */
OUTPUT_FORMAT(elf32-i8085)
ENTRY(_start)

SECTIONS
{
  . = 0x0000;

  .text : {
    *(.text .text.* .init .fini)
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    _sdata = .;
    *(.data .data.*)
    _edata = .;
  }

  .bss (NOLOAD) : {
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    _ebss = .;
  }

  _end = .;
}

PROVIDE(_stack = 0xffff);
PROVIDE(_sidata = LOADADDR(.data));
