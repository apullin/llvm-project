; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs  | FileCheck %s

define void @retvoid(i8* %x) {
; CHECK-LABEL: retvoid:
; CHECK: RET
  ret void
}


define i8 @reteight() #0 {
; CHECK-LABEL: reteight:
; CHECK: MVI	A, 56
; CHECK: RET
  ret i8 56
}

define i16 @retsixteen() {
; CHECK-LABEL: retsixteen:
; CHECK: LXI B, 4660
; CHECK: RET
  ret i16 4660
}

define i32 @retthirtytwo() {
; CHECK-LABEL: retthirtytwo:
; CHECK: MVI M,
; CHECK: RET
  ret i32 16909060
}

define i64 @retsixtyfour() {
; CHECK-LABEL: retsixtyfour:
; CHECK: RET
  ret i64 72623859790382856
}
