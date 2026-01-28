; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Dynamic alloca and stack realignment smoke tests.

define i16 @dynamic_alloca(i16 %n, i8 %x) {
; CHECK-LABEL: dynamic_alloca:
; CHECK: SPHL
; CHECK: RET
entry:
  %count = add i16 %n, 1
  %p = alloca i8, i16 %count, align 1
  %q = getelementptr inbounds i8, ptr %p, i16 %n
  store i8 %x, ptr %q, align 1
  %v = load i8, ptr %q, align 1
  %v16 = zext i8 %v to i16
  ret i16 %v16
}

define i16 @realign_alloca() #0 {
; CHECK-LABEL: realign_alloca:
; CHECK: ANI 252
; CHECK: SPHL
; CHECK: RET
entry:
  %p = alloca i32, align 4
  store i32 0, ptr %p, align 4
  %v = load i32, ptr %p, align 4
  %v16 = trunc i32 %v to i16
  ret i16 %v16
}

attributes #0 = { "stackrealign" }
