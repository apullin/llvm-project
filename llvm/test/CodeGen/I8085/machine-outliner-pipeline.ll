; RUN: llc -mtriple=i8085-unknown-elf -enable-machine-outliner=always -verify-machineinstrs %s -o - | FileCheck %s

target triple = "i8085-unknown-elf"

define i16 @outline0(i16 %x, i16 %y) minsize {
entry:
  %a = add i16 %x, %y
  %b = add i16 %a, 4
  %c = xor i16 %b, 85
  %d = add i16 %c, %y
  ret i16 %d
}

define i16 @outline1(i16 %x, i16 %y) minsize {
entry:
  %a = add i16 %x, %y
  %b = add i16 %a, 4
  %c = xor i16 %b, 85
  %d = add i16 %c, %y
  ret i16 %d
}

; CHECK-LABEL: outline0:
; CHECK: JMP OUTLINED_FUNCTION_0

; CHECK-LABEL: outline1:
; CHECK: JMP OUTLINED_FUNCTION_0

; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK: LXI H, 2
; CHECK: ADD E
; CHECK: XRA L
; CHECK: RET
