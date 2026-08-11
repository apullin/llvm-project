# RUN: llvm-mc -triple=tms9900 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s

  mov @src,@dst
  mov @indexed_src(r1),@indexed_dst(r2)
  li r1,immediate
  xop @xop_target,2
  ldcr @cru_source,8
  mpy @multiply_source,r4

# CHECK:      0x2 R_TMS9900_16 src 0x0
# CHECK-NEXT: 0x4 R_TMS9900_16 dst 0x0
# CHECK-NEXT: 0x8 R_TMS9900_16 indexed_src 0x0
# CHECK-NEXT: 0xA R_TMS9900_16 indexed_dst 0x0
# CHECK-NEXT: 0xE R_TMS9900_16 immediate 0x0
# CHECK-NEXT: 0x12 R_TMS9900_16 xop_target 0x0
# CHECK-NEXT: 0x16 R_TMS9900_16 cru_source 0x0
# CHECK-NEXT: 0x1A R_TMS9900_16 multiply_source 0x0
