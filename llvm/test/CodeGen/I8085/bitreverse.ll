; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i8 @llvm.bitreverse.i8(i8)
declare i16 @llvm.bitreverse.i16(i16)
declare i32 @llvm.bitreverse.i32(i32)

define i8 @bitrev8(i8 %a) {
; CHECK-LABEL: bitrev8:
; CHECK: RET
entry:
  %r = call i8 @llvm.bitreverse.i8(i8 %a)
  ret i8 %r
}

define i16 @bitrev16(i16 %a) {
; CHECK-LABEL: bitrev16:
; CHECK: RET
entry:
  %r = call i16 @llvm.bitreverse.i16(i16 %a)
  ret i16 %r
}

define i32 @bitrev32(i32 %a) {
; CHECK-LABEL: bitrev32:
; CHECK: RET
entry:
  %r = call i32 @llvm.bitreverse.i32(i32 %a)
  ret i32 %r
}
