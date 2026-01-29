SECTIONS
{
  PROVIDE(_stack = 0x8000);
  PROVIDE(_sidata = 0);
  PROVIDE(_sdata = 0);
  PROVIDE(_edata = 0);
  PROVIDE(_sbss = 0);
  PROVIDE(_ebss = 0);

  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
