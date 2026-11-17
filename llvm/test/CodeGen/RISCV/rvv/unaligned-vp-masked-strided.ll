; RUN: llc -mtriple=riscv64 -mattr=+v -verify-machineinstrs < %s | FileCheck %s --check-prefix=STRICT
; RUN: llc -mtriple=riscv64 -mattr=+v,+unaligned-vector-mem -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=UNALIGNED
; RUN: llc -mtriple=riscv32 -mattr=+v -verify-machineinstrs < %s | FileCheck %s --check-prefix=RV32

; Without unaligned vector accesses, masked and strided vector loads and stores
; whose alignment is below the element size use byte-wide accesses.

; STRICT-LABEL: vpload_mask_align1:
; STRICT:         vmerge.vim [[E:v[0-9]+]], {{v[0-9]+}}, -1, v0
; STRICT:         vsetvli {{a[0-9]+}}, zero, e8, m1, ta, ma
; STRICT-NEXT:    vmsne.vi v0, [[E]], 0
; STRICT-NEXT:    slli a1, a1, 2
; STRICT-NEXT:    vsetvli zero, a1, e8, m1, ta, ma
; STRICT-NEXT:    vle8.v v8, (a0), v0.t
; STRICT-NEXT:    ret
; UNALIGNED-LABEL: vpload_mask_align1:
; UNALIGNED:         vle32.v v8, (a0), v0.t

; STRICT-LABEL: vpload_mask_i64_align4:
; STRICT:         vmsne.vi v0, {{v[0-9]+}}, 0
; STRICT-NEXT:    slli a1, a1, 3
; STRICT-NEXT:    vsetvli zero, a1, e8, m1, ta, ma
; STRICT-NEXT:    vle8.v v8, (a0), v0.t
; UNALIGNED-LABEL: vpload_mask_i64_align4:
; UNALIGNED:         vle64.v v8, (a0), v0.t

; STRICT-LABEL: vpload_mask_align4:
; STRICT:         vle32.v v8, (a0), v0.t

; STRICT-LABEL: mload_align1:
; STRICT:         vsetvli {{a[0-9]+}}, zero, e8, m1, ta, mu
; STRICT-NEXT:    vmsne.vi v0, {{v[0-9]+}}, 0
; STRICT-NEXT:    vle8.v v8, (a0), v0.t
; UNALIGNED-LABEL: mload_align1:
; UNALIGNED:         vle32.v v8, (a0), v0.t

; STRICT-LABEL: vpstore_mask_align1:
; STRICT:         vmsne.vi v0, {{v[0-9]+}}, 0
; STRICT-NEXT:    slli a1, a1, 2
; STRICT-NEXT:    vsetvli zero, a1, e8, m1, ta, ma
; STRICT-NEXT:    vse8.v v8, (a0), v0.t
; UNALIGNED-LABEL: vpstore_mask_align1:
; UNALIGNED:         vse32.v v8, (a0), v0.t

; STRICT-LABEL: mstore_align1:
; STRICT:         vmsne.vi v0, {{v[0-9]+}}, 0
; STRICT-NEXT:    vse8.v v8, (a0), v0.t
; UNALIGNED-LABEL: mstore_align1:
; UNALIGNED:         vse32.v v8, (a0), v0.t

; STRICT-LABEL: vpsload_align1:
; STRICT:         vsetvli zero, a2, e8, mf4, ta, ma
; STRICT-NEXT:    vlse8.v {{v[0-9]+}}, (a0), a1, v0.t
; STRICT-COUNT-3: vlse8.v {{v[0-9]+}}, ({{a[0-9]+}}), a1, v0.t
; STRICT-DAG:     vsll.vi {{v[0-9]+}}, {{v[0-9]+}}, 8
; STRICT-DAG:     vsll.vi {{v[0-9]+}}, {{v[0-9]+}}, 16
; STRICT-DAG:     vsll.vi {{v[0-9]+}}, {{v[0-9]+}}, 24
; UNALIGNED-LABEL: vpsload_align1:
; UNALIGNED:         vlse32.v v8, (a0), a1, v0.t

; STRICT-LABEL: vpsload_f32_align2:
; STRICT-COUNT-4: vlse8.v
; UNALIGNED-LABEL: vpsload_f32_align2:
; UNALIGNED:         vlse32.v v8, (a0), a1, v0.t

; STRICT-LABEL: vpsstore_align1:
; STRICT:         slti {{a[0-9]+}}, a1, 0
; STRICT-COUNT-4: vsse8.v {{v[0-9]+}}, ({{a[0-9]+}}), a1, v0.t
; UNALIGNED-LABEL: vpsstore_align1:
; UNALIGNED:         vsse32.v v8, (a0), a1, v0.t

; STRICT-LABEL: vpsstore_align1_stride8:
; STRICT:         addi [[P3:a[0-9]+]], a0, 3
; STRICT:         vnsrl.wi {{v[0-9]+}}, v8, 24
; STRICT:         vsse8.v {{v[0-9]+}}, ([[P3]]), [[S:a[0-9]+]], v0.t
; STRICT:         addi [[P2:a[0-9]+]], a0, 2
; STRICT:         vsse8.v {{v[0-9]+}}, ([[P2]]), [[S]], v0.t
; STRICT:         addi [[P1:a[0-9]+]], a0, 1
; STRICT:         vsse8.v {{v[0-9]+}}, ([[P1]]), [[S]], v0.t
; STRICT-NEXT:    vsse8.v {{v[0-9]+}}, (a0), [[S]], v0.t

; STRICT-LABEL: vpsstore_align1_stride_minus8:
; STRICT:         vsse8.v {{v[0-9]+}}, (a0), [[S:a[0-9]+]], v0.t
; STRICT:         vsse8.v {{v[0-9]+}}, ({{a[0-9]+}}), [[S]], v0.t
; STRICT:         addi [[P3:a[0-9]+]], a0, 3
; STRICT:         vsse8.v {{v[0-9]+}}, ({{a[0-9]+}}), [[S]], v0.t
; STRICT-NEXT:    vsse8.v {{v[0-9]+}}, ([[P3]]), [[S]], v0.t

; STRICT-LABEL: expandload_align1:
; STRICT:         vcpop.m [[N:a[0-9]+]], v0
; STRICT-NEXT:    slli [[N]], [[N]], 2
; STRICT-NEXT:    vsetvli zero, [[N]], e8, m1, ta, ma
; STRICT-NEXT:    vle8.v {{v[0-9]+}}, (a0)
; STRICT:         viota.m
; STRICT:         vrgather.vv v8, {{v[0-9]+}}, {{v[0-9]+}}, v0.t
; UNALIGNED-LABEL: expandload_align1:
; UNALIGNED:         vle32.v {{v[0-9]+}}, (a0)

; STRICT-LABEL: expandload_i64_align1:
; STRICT:         vcpop.m [[N:a[0-9]+]], v0
; STRICT-NEXT:    slli [[N]], [[N]], 3
; STRICT-NEXT:    vsetvli zero, [[N]], e8, m1, ta, ma
; STRICT-NEXT:    vle8.v {{v[0-9]+}}, (a0)
; UNALIGNED-LABEL: expandload_i64_align1:
; UNALIGNED:         vle64.v {{v[0-9]+}}, (a0)

; STRICT-LABEL: expandload_align4:
; STRICT:         vle32.v {{v[0-9]+}}, (a0)

; STRICT-LABEL: compressstore_align1:
; STRICT:         vcompress.vm
; STRICT-NEXT:    vcpop.m [[N:a[0-9]+]], v0
; STRICT-NEXT:    slli [[N]], [[N]], 2
; STRICT-NEXT:    vsetvli zero, [[N]], e8, m1, ta, ma
; STRICT-NEXT:    vse8.v {{v[0-9]+}}, (a0)
; UNALIGNED-LABEL: compressstore_align1:
; UNALIGNED:         vse32.v {{v[0-9]+}}, (a0)

; STRICT-LABEL: compressstore_i64_align1:
; STRICT:         vcompress.vm
; STRICT-NEXT:    vcpop.m [[N:a[0-9]+]], v0
; STRICT-NEXT:    slli [[N]], [[N]], 3
; STRICT-NEXT:    vsetvli zero, [[N]], e8, m1, ta, ma
; STRICT-NEXT:    vse8.v {{v[0-9]+}}, (a0)
; UNALIGNED-LABEL: compressstore_i64_align1:
; UNALIGNED:         vse64.v {{v[0-9]+}}, (a0)

; STRICT-LABEL: compressstore_align4:
; STRICT:         vse32.v {{v[0-9]+}}, (a0)

; STRICT-LABEL: vpsload_align4:
; STRICT:         vlse32.v v8, (a0), a1, v0.t

; STRICT-LABEL: vpsstore_align4:
; STRICT:         vsse32.v v8, (a0), a1, v0.t

; RV32-LABEL: vpsstore_i64_align1:
; RV32:         slti {{a[0-9]+}}, a1, 0
; RV32-COUNT-8: vsse8.v {{v[0-9]+}}, ({{[a-z0-9]+}}), a1, v0.t
; RV32-LABEL: vpsstore_i64_align1_stride16:
; RV32-COUNT-8: vsse8.v {{v[0-9]+}}, ({{[a-z0-9]+}}), {{[a-z0-9]+}}, v0.t
; RV32-LABEL: vpsstore_v4i64_align1:
; RV32:         slti {{a[0-9]+}}, a1, 0
; RV32-COUNT-8: vsse8.v {{v[0-9]+}}, ({{[a-z0-9]+}}), a1, v0.t
; RV32-LABEL: vpsstore_v4i64_align1_stride16:
; RV32-COUNT-8: vsse8.v {{v[0-9]+}}, ({{[a-z0-9]+}}), {{[a-z0-9]+}}, v0.t

define <vscale x 2 x i32> @vpload_mask_align1(ptr %p, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  %v = call <vscale x 2 x i32> @llvm.vp.load.nxv2i32.p0(ptr align 1 %p, <vscale x 2 x i1> %m, i32 %evl)
  ret <vscale x 2 x i32> %v
}

define <vscale x 1 x i64> @vpload_mask_i64_align4(ptr %p, <vscale x 1 x i1> %m, i32 zeroext %evl) {
  %v = call <vscale x 1 x i64> @llvm.vp.load.nxv1i64.p0(ptr align 4 %p, <vscale x 1 x i1> %m, i32 %evl)
  ret <vscale x 1 x i64> %v
}

define <vscale x 2 x i32> @vpload_mask_align4(ptr %p, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  %v = call <vscale x 2 x i32> @llvm.vp.load.nxv2i32.p0(ptr align 4 %p, <vscale x 2 x i1> %m, i32 %evl)
  ret <vscale x 2 x i32> %v
}

define <vscale x 2 x i32> @mload_align1(ptr %p, <vscale x 2 x i1> %m, <vscale x 2 x i32> %passthru) {
  %v = call <vscale x 2 x i32> @llvm.masked.load.nxv2i32.p0(ptr %p, i32 1, <vscale x 2 x i1> %m, <vscale x 2 x i32> %passthru)
  ret <vscale x 2 x i32> %v
}

define void @vpstore_mask_align1(<vscale x 2 x i32> %v, ptr %p, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  call void @llvm.vp.store.nxv2i32.p0(<vscale x 2 x i32> %v, ptr align 1 %p, <vscale x 2 x i1> %m, i32 %evl)
  ret void
}

define void @mstore_align1(<vscale x 2 x i32> %v, ptr %p, <vscale x 2 x i1> %m) {
  call void @llvm.masked.store.nxv2i32.p0(<vscale x 2 x i32> %v, ptr %p, i32 1, <vscale x 2 x i1> %m)
  ret void
}

define <vscale x 2 x i32> @vpsload_align1(ptr %p, i64 %s, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  %v = call <vscale x 2 x i32> @llvm.experimental.vp.strided.load.nxv2i32.p0.i64(ptr align 1 %p, i64 %s, <vscale x 2 x i1> %m, i32 %evl)
  ret <vscale x 2 x i32> %v
}

define <vscale x 2 x float> @vpsload_f32_align2(ptr %p, i64 %s, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  %v = call <vscale x 2 x float> @llvm.experimental.vp.strided.load.nxv2f32.p0.i64(ptr align 2 %p, i64 %s, <vscale x 2 x i1> %m, i32 %evl)
  ret <vscale x 2 x float> %v
}

define void @vpsstore_align1(<vscale x 2 x i32> %v, ptr %p, i64 %s, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.nxv2i32.p0.i64(<vscale x 2 x i32> %v, ptr align 1 %p, i64 %s, <vscale x 2 x i1> %m, i32 %evl)
  ret void
}

define void @vpsstore_align1_stride8(<vscale x 2 x i32> %v, ptr %p, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.nxv2i32.p0.i64(<vscale x 2 x i32> %v, ptr align 1 %p, i64 8, <vscale x 2 x i1> %m, i32 %evl)
  ret void
}

define void @vpsstore_align1_stride_minus8(<vscale x 2 x i32> %v, ptr %p, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.nxv2i32.p0.i64(<vscale x 2 x i32> %v, ptr align 1 %p, i64 -8, <vscale x 2 x i1> %m, i32 %evl)
  ret void
}

define <vscale x 2 x i32> @expandload_align1(ptr %p, <vscale x 2 x i1> %m, <vscale x 2 x i32> %passthru) {
  %v = call <vscale x 2 x i32> @llvm.masked.expandload.nxv2i32(ptr align 1 %p, <vscale x 2 x i1> %m, <vscale x 2 x i32> %passthru)
  ret <vscale x 2 x i32> %v
}

define <vscale x 1 x i64> @expandload_i64_align1(ptr %p, <vscale x 1 x i1> %m, <vscale x 1 x i64> %passthru) {
  %v = call <vscale x 1 x i64> @llvm.masked.expandload.nxv1i64(ptr align 1 %p, <vscale x 1 x i1> %m, <vscale x 1 x i64> %passthru)
  ret <vscale x 1 x i64> %v
}

define <vscale x 2 x i32> @expandload_align4(ptr %p, <vscale x 2 x i1> %m, <vscale x 2 x i32> %passthru) {
  %v = call <vscale x 2 x i32> @llvm.masked.expandload.nxv2i32(ptr align 4 %p, <vscale x 2 x i1> %m, <vscale x 2 x i32> %passthru)
  ret <vscale x 2 x i32> %v
}

define void @compressstore_align1(<vscale x 2 x i32> %v, ptr %p, <vscale x 2 x i1> %m) {
  call void @llvm.masked.compressstore.nxv2i32(<vscale x 2 x i32> %v, ptr align 1 %p, <vscale x 2 x i1> %m)
  ret void
}

define void @compressstore_i64_align1(<vscale x 1 x i64> %v, ptr %p, <vscale x 1 x i1> %m) {
  call void @llvm.masked.compressstore.nxv1i64(<vscale x 1 x i64> %v, ptr align 1 %p, <vscale x 1 x i1> %m)
  ret void
}

define void @compressstore_align4(<vscale x 2 x i32> %v, ptr %p, <vscale x 2 x i1> %m) {
  call void @llvm.masked.compressstore.nxv2i32(<vscale x 2 x i32> %v, ptr align 4 %p, <vscale x 2 x i1> %m)
  ret void
}

define <vscale x 2 x i32> @vpsload_align4(ptr %p, i64 %s, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  %v = call <vscale x 2 x i32> @llvm.experimental.vp.strided.load.nxv2i32.p0.i64(ptr align 4 %p, i64 %s, <vscale x 2 x i1> %m, i32 %evl)
  ret <vscale x 2 x i32> %v
}

define void @vpsstore_align4(<vscale x 2 x i32> %v, ptr %p, i64 %s, <vscale x 2 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.nxv2i32.p0.i64(<vscale x 2 x i32> %v, ptr align 4 %p, i64 %s, <vscale x 2 x i1> %m, i32 %evl)
  ret void
}

define void @vpsstore_i64_align1(<vscale x 1 x i64> %v, ptr %p, i64 %s, <vscale x 1 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.nxv1i64.p0.i64(<vscale x 1 x i64> %v, ptr align 1 %p, i64 %s, <vscale x 1 x i1> %m, i32 %evl)
  ret void
}

define void @vpsstore_i64_align1_stride16(<vscale x 1 x i64> %v, ptr %p, <vscale x 1 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.nxv1i64.p0.i64(<vscale x 1 x i64> %v, ptr align 1 %p, i64 16, <vscale x 1 x i1> %m, i32 %evl)
  ret void
}

define void @vpsstore_v4i64_align1(<4 x i64> %v, ptr %p, i64 %s, <4 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.v4i64.p0.i64(<4 x i64> %v, ptr align 1 %p, i64 %s, <4 x i1> %m, i32 %evl)
  ret void
}

define void @vpsstore_v4i64_align1_stride16(<4 x i64> %v, ptr %p, <4 x i1> %m, i32 zeroext %evl) {
  call void @llvm.experimental.vp.strided.store.v4i64.p0.i64(<4 x i64> %v, ptr align 1 %p, i64 16, <4 x i1> %m, i32 %evl)
  ret void
}
