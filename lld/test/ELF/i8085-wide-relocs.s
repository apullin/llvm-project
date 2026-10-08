# REQUIRES: i8085
# RUN: llvm-mc -triple=i8085 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld -m i8085elf -e 0 --section-start=.data=0x200 --defsym=external=0x1234 --defsym=near=0x234 %t.o -o %t
# RUN: llvm-objdump -s -j .data -j .debug_ref %t | FileCheck %s --check-prefix=DATA
 .data
 .long external + 0x10000
 .quad external - 0x10000
 .long external - .
 .quad external - .
 .short near - .
 .byte near - .
 .section .debug_big,"",@progbits
 .space 65536
debug_end:
 .section .debug_ref,"",@progbits
 .long debug_end
# RELOC: R_I8085_32 external 0x10000
# RELOC: R_I8085_64 external
# RELOC: R_I8085_PC32 external
# RELOC: R_I8085_PC64 external
# RELOC: R_I8085_PC16 near
# RELOC: R_I8085_PC8 near
# RELOC: R_I8085_32 .debug_big 0x10000
# DATA: Contents of section .data:
# DATA: 0200 34120100 3412ffff ffffffff 28100000
# DATA: 0210 24100000 00000000 1c001a
# DATA: Contents of section .debug_ref:
# DATA: 0000 00000100
