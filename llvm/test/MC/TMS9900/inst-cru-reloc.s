; RUN: llvm-mc -triple tms9900 -show-encoding < %s | FileCheck %s --check-prefix=ENC
; RUN: llvm-mc -triple tms9900 -filetype=obj < %s | llvm-readobj -r - | FileCheck %s --check-prefix=REL

  .globl cru_bit
  .globl cru_negative
  .globl byte_value
  sbo cru_bit
  sbz cru_negative
  .byte byte_value

; ENC: SBO{{[ \t]+}}cru_bit
; ENC: fixup A - offset: 1, value: cru_bit, kind: fixup_tms9900_8
; ENC: SBZ{{[ \t]+}}cru_negative
; ENC: fixup A - offset: 1, value: cru_negative, kind: fixup_tms9900_8

; REL: 0x1 R_TMS9900_CRU_8 cru_bit 0x0
; REL: 0x3 R_TMS9900_CRU_8 cru_negative 0x0
; REL: 0x4 R_TMS9900_8 byte_value 0x0
