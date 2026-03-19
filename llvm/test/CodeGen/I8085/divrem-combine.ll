; RUN: llc -O2 -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-none-unknown"

define void @qr13(ptr %out, i16 %x) {
; CHECK-LABEL: qr13:
; CHECK: LXI B, 13
; CHECK: LXI H, 2
; CHECK-NEXT: DAD SP
; CHECK-NEXT: MOV M, C
; CHECK-NEXT: INX H
; CHECK-NEXT: MOV M, B
; CHECK: LXI H, 0
; CHECK-NEXT: DAD SP
; CHECK-NEXT: MOV M, C
; CHECK-NEXT: INX H
; CHECK-NEXT: MOV M, B
; CHECK: CALL __udivmod16
; CHECK: MOV A, C
; CHECK: STAX B
; CHECK: MOV A, E
entry:
  %q = udiv i16 %x, 13
  %qt = trunc i16 %q to i8
  store volatile i8 %qt, ptr %out
  %r = urem i16 %x, 13
  %rt = trunc i16 %r to i8
  %out1 = getelementptr inbounds i8, ptr %out, i16 1
  store volatile i8 %rt, ptr %out1
  ret void
}
