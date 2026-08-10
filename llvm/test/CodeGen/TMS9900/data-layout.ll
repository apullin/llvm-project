; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; Keep LLVM's standalone IR layout consistent with Clang's 16-bit scalar ABI.

%float_record = type { i8, float, i8 }
%wide_record = type { i8, i64, i8 }
%double_record = type { i8, double, i8 }

; CHECK-LABEL: float_field:
; CHECK: INCT{{[ \t]+}}R0
define ptr @float_field(ptr %record) {
  %field = getelementptr %float_record, ptr %record, i16 0, i32 1
  ret ptr %field
}

; CHECK-LABEL: float_next:
; CHECK: AI{{[ \t]+}}R0,8
define ptr @float_next(ptr %record) {
  %next = getelementptr %float_record, ptr %record, i16 1
  ret ptr %next
}

; CHECK-LABEL: wide_field:
; CHECK: INCT{{[ \t]+}}R0
define ptr @wide_field(ptr %record) {
  %field = getelementptr %wide_record, ptr %record, i16 0, i32 1
  ret ptr %field
}

; CHECK-LABEL: wide_next:
; CHECK: AI{{[ \t]+}}R0,12
define ptr @wide_next(ptr %record) {
  %next = getelementptr %wide_record, ptr %record, i16 1
  ret ptr %next
}

; CHECK-LABEL: double_field:
; CHECK: INCT{{[ \t]+}}R0
define ptr @double_field(ptr %record) {
  %field = getelementptr %double_record, ptr %record, i16 0, i32 1
  ret ptr %field
}

; CHECK-LABEL: double_next:
; CHECK: AI{{[ \t]+}}R0,12
define ptr @double_next(ptr %record) {
  %next = getelementptr %double_record, ptr %record, i16 1
  ret ptr %next
}
