; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

; Indirect loads/stores via pointers and offsets.

define void @mem_indirect(ptr %p8, ptr %p16, i8 %x, i16 %y) {
; CHECK-LABEL: mem_indirect:
; CHECK: RET
entry:
  store volatile i8 %x, ptr %p8, align 1
  store volatile i16 %y, ptr %p16, align 1
  %p8o = getelementptr inbounds i8, ptr %p8, i16 1
  %p16o = getelementptr inbounds i16, ptr %p16, i16 1
  %lv8 = load volatile i8, ptr %p8o, align 1
  %lv16 = load volatile i16, ptr %p16o, align 1
  %lv8_16 = zext i8 %lv8 to i16
  %sum = add i16 %lv16, %lv8_16
  store volatile i16 %sum, ptr %p16o, align 1
  ret void
}
