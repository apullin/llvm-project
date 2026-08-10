# REQUIRES: tms9900

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -triple tms9900 -filetype=obj success.s -o success.o
# RUN: llvm-readobj -r success.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld -e start -Ttext=0 success.o -o success \
# RUN:   --defsym=cru_bit=5 --defsym=cru_negative=-1 \
# RUN:   --defsym=near_target=8 --defsym=byte_value=0x7a
# RUN: llvm-objdump -s success | FileCheck %s --check-prefix=LINKED
# RUN: llvm-mc -triple tms9900 -filetype=obj odd.s -o odd.o
# RUN: not ld.lld -Ttext=0 odd.o -o /dev/null --defsym=odd_target=5 2>&1 | \
# RUN:   FileCheck %s --check-prefix=ODD
# RUN: llvm-mc -triple tms9900 -filetype=obj far.s -o far.o
# RUN: not ld.lld -Ttext=0 far.o -o /dev/null --defsym=far_target=0x1000 2>&1 | \
# RUN:   FileCheck %s --check-prefix=FAR
# RUN: llvm-mc -triple tms9900 -filetype=obj cru-range.s -o cru-range.o
# RUN: not ld.lld -Ttext=0 cru-range.o -o /dev/null --defsym=cru_bit=128 2>&1 | \
# RUN:   FileCheck %s --check-prefix=CRU-RANGE

# RELOC: 0x1 R_TMS9900_CRU_8 cru_bit 0x0
# RELOC: 0x3 R_TMS9900_CRU_8 cru_negative 0x0
# RELOC: 0x4 R_TMS9900_PCREL_8 near_target 0x0
# RELOC: 0x6 R_TMS9900_8 byte_value 0x0

# LINKED: Contents of section .text:
# LINKED-NEXT: 0000 1d051eff 13017a

# ODD: error: {{.*}}improper alignment for relocation R_TMS9900_PCREL_8: 0x5 is not aligned to 2 bytes
# FAR: error: {{.*}}relocation R_TMS9900_PCREL_8 out of range: 2047 is not in [-128, 127]
# CRU-RANGE: error: {{.*}}relocation R_TMS9900_CRU_8 out of range: 128 is not in [-128, 127]

#--- success.s
  .text
  .globl start
start:
  sbo cru_bit
  sbz cru_negative
  jeq near_target
  .byte byte_value

#--- odd.s
  .text
  jeq odd_target

#--- far.s
  .text
  jeq far_target

#--- cru-range.s
  .text
  sbo cru_bit
