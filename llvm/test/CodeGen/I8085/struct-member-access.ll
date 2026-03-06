; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Nested struct access patterns via pointer.
; Adapted from generic RISC-V memory access patterns.

%struct.Point = type { i16, i16 }
%struct.Rect = type { %struct.Point, %struct.Point }

; Access first struct member (offset 0) via pointer
define i16 @get_point_x(%struct.Point* %p) {
; CHECK-LABEL: get_point_x:
; CHECK: DAD SP
; CHECK: MOV B, M
; CHECK-NOT: ADD
; CHECK: MOV C, M
; CHECK: INX H
; CHECK: MOV B, M
; CHECK: RET
entry:
  %xptr = getelementptr %struct.Point, %struct.Point* %p, i16 0, i32 0
  %x = load i16, i16* %xptr
  ret i16 %x
}

; Access second struct member (offset 2) via pointer
define i16 @get_point_y(%struct.Point* %p) {
; CHECK-LABEL: get_point_y:
; CHECK: LXI B, 2
; CHECK: ADD C
; CHECK: ADC B
; CHECK: LDAX D
; CHECK: INX D
; CHECK: LDAX D
; CHECK: RET
entry:
  %yptr = getelementptr %struct.Point, %struct.Point* %p, i16 0, i32 1
  %y = load i16, i16* %yptr
  ret i16 %y
}

; Nested struct: access inner struct at offset 4
define i16 @get_rect_x2(%struct.Rect* %r) {
; CHECK-LABEL: get_rect_x2:
; CHECK: LXI B, 4
; CHECK: ADD C
; CHECK: ADC B
; CHECK: LDAX D
; CHECK: INX D
; CHECK: LDAX D
; CHECK: RET
entry:
  %p2ptr = getelementptr %struct.Rect, %struct.Rect* %r, i16 0, i32 1
  %xptr = getelementptr %struct.Point, %struct.Point* %p2ptr, i16 0, i32 0
  %x = load i16, i16* %xptr
  ret i16 %x
}

; Store to struct members
define void @set_point(%struct.Point* %p, i16 %x, i16 %y) {
; CHECK-LABEL: set_point:
; CHECK: MOV M, A
; CHECK: RET
entry:
  %xptr = getelementptr %struct.Point, %struct.Point* %p, i16 0, i32 0
  store i16 %x, i16* %xptr
  %yptr = getelementptr %struct.Point, %struct.Point* %p, i16 0, i32 1
  store i16 %y, i16* %yptr
  ret void
}
