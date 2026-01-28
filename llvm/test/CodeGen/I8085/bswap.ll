; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare i16 @llvm.bswap.i16(i16)
declare i32 @llvm.bswap.i32(i32)

define i16 @bswap16(i16 %a) {
; CHECK-LABEL: bswap16:
; CHECK: RET
entry:
  %r = call i16 @llvm.bswap.i16(i16 %a)
  ret i16 %r
}

define i32 @bswap32(i32 %a) {
; CHECK-LABEL: bswap32:
; CHECK: RET
entry:
  %r = call i32 @llvm.bswap.i32(i32 %a)
  ret i32 %r
}
