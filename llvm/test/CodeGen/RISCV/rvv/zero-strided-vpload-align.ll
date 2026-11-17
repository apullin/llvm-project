; RUN: opt -S -riscv-codegenprepare -mtriple=riscv64 -mattr=+v < %s | FileCheck %s --check-prefix=IR
; RUN: llc -mtriple=riscv64 -mattr=+v < %s | FileCheck %s --check-prefix=STRICT
; RUN: llc -mtriple=riscv64 -mattr=+v,+unaligned-scalar-mem < %s | FileCheck %s --check-prefix=UNALIGNED

; A zero-strided vp.strided.load becomes a scalar load and a splat. The scalar
; load has the alignment of the vp.strided.load's pointer, or the element's ABI
; alignment if it has none.

; IR-LABEL: @align1(
; IR:         load i32, ptr %p, align 1
; IR-LABEL: @align2(
; IR:         load i32, ptr %p, align 2
; IR-LABEL: @no_align(
; IR:         load i32, ptr %p, align 4

; STRICT-LABEL: align1:
; STRICT-NOT:     lw
; STRICT-COUNT-4: {{lbu?}} a{{[0-9]+}}, {{[0-3]}}(a0)
; STRICT-NOT:     lw
; STRICT:         vmv.v.x
; STRICT-LABEL: align2:
; STRICT-NOT:     lw
; STRICT-COUNT-2: {{lhu?}} a{{[0-9]+}}, {{[02]}}(a0)
; STRICT-NOT:     lw
; STRICT:         vmv.v.x
; STRICT-LABEL: no_align:
; STRICT:         lw a0, 0(a0)
; STRICT:         vmv.v.x

; UNALIGNED-LABEL: align1:
; UNALIGNED:         lw a0, 0(a0)
; UNALIGNED:         vmv.v.x

define <vscale x 2 x i32> @align1(ptr %p) {
  %v = call <vscale x 2 x i32> @llvm.experimental.vp.strided.load.nxv2i32.p0.i64(ptr align 1 %p, i64 0, <vscale x 2 x i1> splat (i1 true), i32 4)
  ret <vscale x 2 x i32> %v
}

define <vscale x 2 x i32> @align2(ptr %p) {
  %v = call <vscale x 2 x i32> @llvm.experimental.vp.strided.load.nxv2i32.p0.i64(ptr align 2 %p, i64 0, <vscale x 2 x i1> splat (i1 true), i32 4)
  ret <vscale x 2 x i32> %v
}

define <vscale x 2 x i32> @no_align(ptr %p) {
  %v = call <vscale x 2 x i32> @llvm.experimental.vp.strided.load.nxv2i32.p0.i64(ptr %p, i64 0, <vscale x 2 x i1> splat (i1 true), i32 4)
  ret <vscale x 2 x i32> %v
}
