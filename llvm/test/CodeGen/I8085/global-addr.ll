; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s

@g8 = global i8 0
@g16 = global i16 0

; Global addressing should lower via absolute address materialization.

define void @store_globals(i8 %a, i16 %b) {
; CHECK-LABEL: store_globals:
; CHECK: LXI D, g16
; CHECK: STAX D
; CHECK: INX D
; CHECK: STAX D
; CHECK: LXI D, g8
; CHECK: STAX D
entry:
  store i8 %a, i8* @g8, align 1
  store i16 %b, i16* @g16, align 1
  ret void
}

define i16 @load_globals() {
; CHECK-LABEL: load_globals:
; CHECK: LXI H, g8
; CHECK: MOV B, M
; CHECK: LXI H, g16+1
; CHECK: MOV D, M
; CHECK: LXI H, g16
; CHECK: MOV E, M
; CHECK: RET
entry:
  %a = load i8, i8* @g8, align 1
  %b = load i16, i16* @g16, align 1
  %a16 = zext i8 %a to i16
  %sum = add i16 %a16, %b
  ret i16 %sum
}
