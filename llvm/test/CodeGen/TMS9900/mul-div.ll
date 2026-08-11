; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=tms9900 -O2 -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=tms9900 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=tms9900 -O2 -filetype=obj < %s -o /dev/null
;
; Test multiply and divide operations.
; TMS9900 has hardware MPY (unsigned multiply) and DIV (unsigned divide).

; --- 16-bit unsigned multiply ---
; Uses MPY instruction.
; CHECK-LABEL: mul16:
; CHECK: MPY
; CHECK: B{{[ \t]+}}*R11
; ISEL-LABEL: name: mul16
; ISEL: MPYrr_R0 {{.*}}implicit-def $r0, implicit-def $r1, implicit $r0

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

; DIV sets the architectural overflow bit on failure and clears it on success,
; so the scheduler must see an ST definition on the selected instruction.
; ISEL-LABEL: name: udiv16
; ISEL: DIVrr_R0 {{.*}}implicit-def $r0, implicit-def $r1, implicit-def $st, implicit $r0, implicit $r1

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

; Signed division computes operand magnitudes with ABS and carries values across
; the sign-correction branch in virtual registers. In particular, no allocatable
; physical register may be declared live-in to the custom-inserter blocks.
; ISEL-LABEL: name: sdiv16
; ISEL: ABSr
; ISEL: ABSr
; ISEL: DIVrr
; ISEL: CMPBRri %{{[0-9]+}}, 0, 20,

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
; ISEL: ABSr
; ISEL: ABSr
; ISEL: DIVrr
; ISEL: CMPBRri %{{[0-9]+}}, 0, 20,
