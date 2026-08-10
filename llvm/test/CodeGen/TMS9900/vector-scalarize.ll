; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
;
; TMS9900 has no vector registers.  Vector operations must remain well-typed
; while SelectionDAG splits them into scalar i16 operations.  In particular,
; signed division by a constant introduces vector comparisons before type
; legalization.

; CHECK-LABEL: sdiv_v8i16:
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: B{{[ \t]+}}*R11
define void @sdiv_v8i16(ptr %dst, ptr %src) {
  %v = load <8 x i16>, ptr %src, align 2
  %q = sdiv <8 x i16> %v, splat (i16 4)
  store <8 x i16> %q, ptr %dst, align 2
  ret void
}

; CHECK-LABEL: srem_v8i16:
; CHECK: ANDI
; CHECK: ANDI
; CHECK: ANDI
; CHECK: ANDI
; CHECK: ANDI
; CHECK: ANDI
; CHECK: ANDI
; CHECK: ANDI
; CHECK: B{{[ \t]+}}*R11
define void @srem_v8i16(ptr %dst, ptr %src) {
  %v = load <8 x i16>, ptr %src, align 2
  %r = srem <8 x i16> %v, splat (i16 4)
  store <8 x i16> %r, ptr %dst, align 2
  ret void
}

; CHECK-LABEL: abs_v8i16:
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: SRA
; CHECK: B{{[ \t]+}}*R11
define void @abs_v8i16(ptr %dst, ptr %src) {
  %v = load <8 x i16>, ptr %src, align 2
  %negative = icmp slt <8 x i16> %v, zeroinitializer
  %negated = sub <8 x i16> zeroinitializer, %v
  %abs = select <8 x i1> %negative, <8 x i16> %negated, <8 x i16> %v
  store <8 x i16> %abs, ptr %dst, align 2
  ret void
}
