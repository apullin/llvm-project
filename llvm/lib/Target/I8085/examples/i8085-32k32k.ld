/* Example 32K ROM / 32K RAM linker script for i8085. */
OUTPUT_FORMAT(elf32-i8085)
ENTRY(_start)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x0000, LENGTH = 0x8000
  RAM   (rwx) : ORIGIN = 0x8000, LENGTH = 0x8000
}

SECTIONS
{
  .vectors : {
    KEEP(*(.vectors))
  } > FLASH

  .text : {
    *(.text .text.* .init .fini)
  } > FLASH

  .rodata : {
    *(.rodata .rodata.*)
  } > FLASH

  . = ALIGN(2);
  _etext = .;

  .data : AT (_etext) {
    . = ALIGN(2);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(2);
    _edata = .;
  } > RAM

  _sidata = LOADADDR(.data);

  .bss (NOLOAD) : {
    . = ALIGN(2);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(2);
    _ebss = .;
  } > RAM

  _end = .;
}

PROVIDE(_estack = ORIGIN(RAM) + LENGTH(RAM));
PROVIDE(_stack = _estack);
