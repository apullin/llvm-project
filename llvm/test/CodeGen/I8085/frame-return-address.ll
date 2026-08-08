; RUN: llc -mtriple=i8085-unknown-elf -verify-machineinstrs < %s | FileCheck %s

declare ptr @llvm.returnaddress(i32 immarg)
declare ptr @llvm.frameaddress.p0(i32 immarg)

define ptr @return_address_0() {
; CHECK-LABEL: return_address_0:
; CHECK:       LXI H, 0
; CHECK-NEXT:  DAD SP
; CHECK-NEXT:  MOV C, M
; CHECK-NEXT:  INX H
; CHECK-NEXT:  MOV B, M
; CHECK-NEXT:  RET
  %address = call ptr @llvm.returnaddress(i32 0)
  ret ptr %address
}

define ptr @frame_address_0() {
; CHECK-LABEL: frame_address_0:
; CHECK:       LXI H, 0
; CHECK-NEXT:  DAD SP
; CHECK-NEXT:  MOV B, H
; CHECK-NEXT:  MOV C, L
; CHECK-NEXT:  RET
  %address = call ptr @llvm.frameaddress.p0(i32 0)
  ret ptr %address
}

define ptr @return_address_1() {
; CHECK-LABEL: return_address_1:
; CHECK:       LXI B, 0
; CHECK-NEXT:  RET
  %address = call ptr @llvm.returnaddress(i32 1)
  ret ptr %address
}

define ptr @frame_address_1() {
; CHECK-LABEL: frame_address_1:
; CHECK:       LXI B, 0
; CHECK-NEXT:  RET
  %address = call ptr @llvm.frameaddress.p0(i32 1)
  ret ptr %address
}
