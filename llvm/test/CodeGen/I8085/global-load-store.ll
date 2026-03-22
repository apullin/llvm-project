; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Global variable access patterns: load, modify, store.
; Adapted from RISC-V remat.ll and global patterns.

@counter = global i16 0
@flag = global i8 0
@array = global [4 x i16] zeroinitializer

; Load, modify, store a global
define void @increment_global() {
; CHECK-LABEL: increment_global:
; CHECK: LXI B, 1
; CHECK: LXI H, counter+1
; CHECK: MOV D, M
; CHECK: LXI H, counter
; CHECK: MOV E, M
; CHECK: ADD C
; CHECK: ADC B
; CHECK: LXI H, counter
; CHECK: MOV M, E
; CHECK: INX H
; CHECK: MOV M, D
; CHECK: RET
entry:
  %val = load i16, i16* @counter
  %new = add i16 %val, 1
  store i16 %new, i16* @counter
  ret void
}

; Store to global i8
define void @set_flag(i8 %v) {
; CHECK-LABEL: set_flag:
; CHECK: DAD SP
; CHECK: MOV A, M
; CHECK: LXI H, flag
; CHECK: MOV M, A
; CHECK: RET
entry:
  store i8 %v, i8* @flag
  ret void
}

; Load from global i8
define i8 @get_flag() {
; CHECK-LABEL: get_flag:
; CHECK: LXI H, flag
; CHECK: MOV A, M
; CHECK: RET
entry:
  %v = load i8, i8* @flag
  ret i8 %v
}

; Access global array element via GEP (index * 2 + base)
define i16 @load_array_elem(i16 %idx) {
; CHECK-LABEL: load_array_elem:
; CHECK: LXI B, array
; CHECK: ADD C
; CHECK: ADC B
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: RET
entry:
  %ptr = getelementptr [4 x i16], [4 x i16]* @array, i16 0, i16 %idx
  %val = load i16, i16* %ptr
  ret i16 %val
}

; Store to global array element
define void @store_array_elem(i16 %idx, i16 %val) {
; CHECK-LABEL: store_array_elem:
; CHECK: LXI B, array
; CHECK: ADD C
; CHECK: ADC B
; CHECK: STAX D
; CHECK: RET
entry:
  %ptr = getelementptr [4 x i16], [4 x i16]* @array, i16 0, i16 %idx
  store i16 %val, i16* %ptr
  ret void
}
