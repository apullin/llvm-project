; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

declare { i16, i1 } @llvm.sadd.with.overflow.i16(i16, i16)
declare { i8, i1 } @llvm.uadd.with.overflow.i8(i8, i8)

define i16 @sadd_ov_i16(i16 %a, i16 %b) {
; CHECK-LABEL: sadd_ov_i16:
; CHECK: RET
entry:
  %res = call { i16, i1 } @llvm.sadd.with.overflow.i16(i16 %a, i16 %b)
  %sum = extractvalue { i16, i1 } %res, 0
  ret i16 %sum
}

define i1 @sadd_ov_flag(i16 %a, i16 %b) {
; CHECK-LABEL: sadd_ov_flag:
; CHECK: RET
entry:
  %res = call { i16, i1 } @llvm.sadd.with.overflow.i16(i16 %a, i16 %b)
  %ov = extractvalue { i16, i1 } %res, 1
  ret i1 %ov
}

define i8 @uadd_ov_i8(i8 %a, i8 %b) {
; CHECK-LABEL: uadd_ov_i8:
; CHECK: RET
entry:
  %res = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %a, i8 %b)
  %sum = extractvalue { i8, i1 } %res, 0
  ret i8 %sum
}

define i1 @uadd_ov_flag(i8 %a, i8 %b) {
; CHECK-LABEL: uadd_ov_flag:
; CHECK: RET
entry:
  %res = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %a, i8 %b)
  %ov = extractvalue { i8, i1 } %res, 1
  ret i1 %ov
}
