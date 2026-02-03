; RUN: opt < %s -passes="print<cost-model>" -mtriple=i8085-unknown-elf 2>&1 -disable-output | FileCheck %s

; CHECK-LABEL: function 'add_i8'
; CHECK: cost of 1 {{.*}} add i8
; CHECK: cost of 1 {{.*}} ret i8

define i8 @add_i8(i8 %a, i8 %b) {
  %r = add i8 %a, %b
  ret i8 %r
}

; CHECK-LABEL: function 'add_i32'
; CHECK: cost of 4 {{.*}} add i32
; CHECK: cost of 1 {{.*}} ret i32

define i32 @add_i32(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: function 'mul_i32'
; CHECK: cost of 128 {{.*}} mul i32
; CHECK: cost of 1 {{.*}} ret i32

define i32 @mul_i32(i32 %a, i32 %b) {
  %r = mul i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: function 'load_i32'
; CHECK: cost of 4 {{.*}} load i32
; CHECK: cost of 1 {{.*}} ret i32

define i32 @load_i32(ptr %p) {
  %v = load i32, ptr %p
  ret i32 %v
}

; CHECK-LABEL: function 'trunc_i32_to_i8'
; CHECK: cost of 0 {{.*}} trunc i32
; CHECK: cost of 1 {{.*}} ret i8

define i8 @trunc_i32_to_i8(i32 %a) {
  %t = trunc i32 %a to i8
  ret i8 %t
}
