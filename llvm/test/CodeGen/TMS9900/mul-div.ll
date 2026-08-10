; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=tms9900 -O2 -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ISEL
;
; Test multiply and divide operations.
; TMS9900 has hardware MPY (unsigned multiply) and DIV (unsigned divide).

; --- 16-bit unsigned multiply ---
; Uses MPY instruction.
; CHECK-LABEL: mul16:
; CHECK: MPY
; CHECK: B{{[ \t]+}}*R11

define i16 @mul16(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  ret i16 %r
}

; --- 16-bit unsigned divide ---
; Uses DIV instruction, quotient in first register of pair.
; CHECK-LABEL: udiv16:
; CHECK: DIV
; CHECK: B{{[ \t]+}}*R11

define i16 @udiv16(i16 %a, i16 %b) {
  %r = udiv i16 %a, %b
  ret i16 %r
}

; --- 16-bit unsigned remainder ---
; Uses DIV instruction, remainder in second register of pair.
; CHECK-LABEL: urem16:
; CHECK: DIV
; CHECK: B{{[ \t]+}}*R11

define i16 @urem16(i16 %a, i16 %b) {
  %r = urem i16 %a, %b
  ret i16 %r
}

; --- 16-bit signed divide ---
; Requires sign handling around DIV since TMS9900 DIV is unsigned.
; CHECK-LABEL: sdiv16:
; CHECK: DIV
; CHECK: B{{[ \t]+}}*R11

define i16 @sdiv16(i16 %a, i16 %b) {
  %r = sdiv i16 %a, %b
  ret i16 %r
}

; Signed division tracks both operand signs and the result sign with three
; compare/branch pairs. They must remain fused until after scheduling because
; almost every TMS9900 data-movement instruction also changes ST.
; ISEL-LABEL: name: sdiv16
; ISEL: CMPBRri $r1, 0, 20,
; ISEL: CMPBRri $r3, 0, 20,
; ISEL: CMPBRri $r2, 0, 22,

; --- 16-bit signed remainder ---
; Remainder sign follows the dividend.
; CHECK-LABEL: srem16:
; CHECK: DIV
; CHECK: B{{[ \t]+}}*R11

define i16 @srem16(i16 %a, i16 %b) {
  %r = srem i16 %a, %b
  ret i16 %r
}

; ISEL-LABEL: name: srem16
; ISEL: CMPBRri $r1, 0, 20,
; ISEL: CMPBRri $r3, 0, 20,
; ISEL: CMPBRri $r2, 0, 22,
