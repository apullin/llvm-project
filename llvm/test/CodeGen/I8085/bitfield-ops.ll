; RUN: llc -mattr=i8085,sram < %s -march=i8085 | FileCheck %s

; Bitfield operations: extract, insert, mask-and-shift.
; Adapted from RISC-V and.ll/narrow-shl-cst.ll patterns.

; Extract bits [3:0] from i8
define i8 @extract_low_nibble(i8 %x) {
; CHECK-LABEL: extract_low_nibble:
; CHECK: ANI 15
; CHECK: RET
entry:
  %r = and i8 %x, 15
  ret i8 %r
}

; Extract bits [7:4] from i8 (logical shift right by 4)
define i8 @extract_high_nibble(i8 %x) {
; CHECK-LABEL: extract_high_nibble:
; CHECK: RRC
; CHECK: ANI 127
; CHECK: RRC
; CHECK: ANI 127
; CHECK: RRC
; CHECK: ANI 127
; CHECK: RRC
; CHECK: ANI 127
; CHECK: RET
entry:
  %shifted = lshr i8 %x, 4
  %r = and i8 %shifted, 15
  ret i8 %r
}

; Insert value into bits [7:4] of target (clearing them first)
define i8 @insert_high_nibble(i8 %target, i8 %value) {
; CHECK-LABEL: insert_high_nibble:
; CHECK: ANI 15
; CHECK: ADD A
; CHECK: ADD A
; CHECK: ADD A
; CHECK: ADD A
; CHECK: ORA C
; CHECK: RET
entry:
  %cleared = and i8 %target, 15
  %shifted = shl i8 %value, 4
  %r = or i8 %cleared, %shifted
  ret i8 %r
}

; Set single bit (bit 3) using ORI
define i8 @set_bit3(i8 %x) {
; CHECK-LABEL: set_bit3:
; CHECK: ORI 8
; CHECK: RET
entry:
  %r = or i8 %x, 8
  ret i8 %r
}

; Clear single bit (bit 3) using ANI
define i8 @clear_bit3(i8 %x) {
; CHECK-LABEL: clear_bit3:
; CHECK: ANI -9
; CHECK: RET
entry:
  %r = and i8 %x, -9
  ret i8 %r
}

; Toggle single bit (bit 3) using XRI
define i8 @toggle_bit3(i8 %x) {
; CHECK-LABEL: toggle_bit3:
; CHECK: XRI 8
; CHECK: RET
entry:
  %r = xor i8 %x, 8
  ret i8 %r
}

; Combine two i8 into i16 (pack hi:lo) — should be a simple swap
define i16 @pack_i8_to_i16(i8 %hi, i8 %lo) {
; CHECK-LABEL: pack_i8_to_i16:
; CHECK: MOV C, B
; CHECK: MOV B, A
; CHECK: RET
entry:
  %hi16 = zext i8 %hi to i16
  %lo16 = zext i8 %lo to i16
  %shifted = shl i16 %hi16, 8
  %r = or i16 %shifted, %lo16
  ret i16 %r
}
