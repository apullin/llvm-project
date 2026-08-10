; RUN: llc -march=tms9900 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; Test dynamic stack allocation (variable-length arrays).
; DYNAMIC_STACKALLOC is Expand on TMS9900.
; The expansion computes the byte size (n * element_size), then
; subtracts from the stack pointer (R10) to allocate space.

; --- Variable-length allocation and store ---
; Allocates n * 2 bytes on the stack (i16 elements), stores a value.
; SLA R0,1 computes byte count, then adjusts R10 (stack pointer).
; CHECK-LABEL: vla:
; CHECK: AI{{[ \t]+}}R10,-4
; CHECK-NEXT: MOV{{[ \t]+}}R13,@2(R10)
; CHECK-NEXT: MOV{{[ \t]+}}R10,R13
; CHECK: SLA{{[ \t]+}}R0,1
; CHECK: S{{[ \t]+}}R{{[0-9]+}},R{{[0-9]+}}
; CHECK: MOV{{[ \t]+}}R{{[0-9]+}},R10
; CHECK: MOV{{[ \t]+}}R13,R10
; CHECK-NEXT: MOV{{[ \t]+}}@2(R10),R13
; CHECK-NEXT: AI{{[ \t]+}}R10,4
; CHECK: B{{[ \t]+}}*R11

define void @vla(i16 %n) {
  %p = alloca i16, i16 %n
  store i16 42, ptr %p
  ret void
}

; --- Variable-length allocation with load back ---
; Allocates, stores, then loads back from the dynamically allocated area.
; CHECK-LABEL: dynalloca_read:
; CHECK: MOV{{[ \t]+}}R10,R13
; CHECK: SLA{{[ \t]+}}R0,1
; CHECK: S{{[ \t]+}}R{{[0-9]+}},R{{[0-9]+}}
; CHECK: MOV{{[ \t]+}}R{{[0-9]+}},R10
; CHECK: MOV{{[ \t]+}}R13,R10
; CHECK-NEXT: MOV{{[ \t]+}}@2(R10),R13
; CHECK-NEXT: AI{{[ \t]+}}R10,4
; CHECK: B{{[ \t]+}}*R11

define i16 @dynalloca_read(i16 %n) {
  %p = alloca i16, i16 %n
  store i16 100, ptr %p
  %v = load i16, ptr %p
  ret i16 %v
}

; --- Fixed and variable objects across a call ---
; Fixed frame references must remain based on R13 while R10 tracks the VLA and
; temporary outgoing call frame.
; CHECK-LABEL: dynalloca_with_call:
; CHECK: DECT{{[ \t]+}}R10
; CHECK: MOV{{[ \t]+}}R11,*R10
; CHECK: AI{{[ \t]+}}R10,-{{[0-9]+}}
; CHECK: MOV{{[ \t]+}}R13,@{{[0-9]+}}(R10)
; CHECK: MOV{{[ \t]+}}R10,R13
; CHECK: MOV{{[ \t]+}}R14,@{{[0-9]+}}(R13)
; CHECK-NEXT: MOV{{[ \t]+}}R13,R14
; CHECK-NEXT: AI{{[ \t]+}}R14,{{[0-9]+}}
; CHECK: MOV{{[ \t]+}}R1,*R14
; CHECK: MOV{{[ \t]+}}R{{[0-9]+}},R10
; CHECK: MOV{{[ \t]+}}*R14,R0
; CHECK: DECT{{[ \t]+}}R10
; CHECK: MOV{{[ \t]+}}R{{[0-9]+}},*R10
; CHECK: BL{{[ \t]+}}@consume
; CHECK: INCT{{[ \t]+}}R10
; CHECK: MOV{{[ \t]+}}*R14,R0
; CHECK: MOV{{[ \t]+}}@{{[0-9]+}}(R13),R14
; CHECK: MOV{{[ \t]+}}R13,R10
; CHECK: MOV{{[ \t]+}}@{{[0-9]+}}(R10),R13
; CHECK: AI{{[ \t]+}}R10,{{[0-9]+}}
; CHECK: MOV{{[ \t]+}}*R10+,R11
; CHECK: B{{[ \t]+}}*R11

declare void @consume(i16, i16, i16, i16, ptr)

define i16 @dynalloca_with_call(i16 %n, i16 %value) {
  %fixed = alloca i16, align 2
  store volatile i16 %value, ptr %fixed, align 2
  %dynamic = alloca i8, i16 %n, align 2
  store volatile i8 7, ptr %dynamic, align 1
  %argument = load volatile i16, ptr %fixed, align 2
  call void @consume(i16 %argument, i16 2, i16 3, i16 4, ptr %dynamic)
  %result = load volatile i16, ptr %fixed, align 2
  ret i16 %result
}

; --- llvm.frameaddress(0) ---
; Taking the frame address also establishes and returns the stable R13 base.
; CHECK-LABEL: frame_address:
; CHECK: MOV{{[ \t]+}}R10,R13
; CHECK: MOV{{[ \t]+}}R13,R0
; CHECK: MOV{{[ \t]+}}R13,R10
; CHECK: B{{[ \t]+}}*R11

declare ptr @llvm.frameaddress(i32 immarg)

define ptr @frame_address() {
  %address = call ptr @llvm.frameaddress(i32 0)
  ret ptr %address
}

; --- Explicitly retained frame pointer ---
; The standard frame-pointer function attribute must use the same R13 save and
; restore convention even without a VLA.
; CHECK-LABEL: retained_frame_pointer:
; CHECK: MOV{{[ \t]+}}R13,@{{[0-9]+}}(R10)
; CHECK: MOV{{[ \t]+}}R10,R13
; CHECK: BL{{[ \t]+}}@callee
; CHECK: MOV{{[ \t]+}}R13,R10
; CHECK: MOV{{[ \t]+}}@{{[0-9]+}}(R10),R13
; CHECK: B{{[ \t]+}}*R11

declare void @callee()

define void @retained_frame_pointer() #0 {
  call void @callee()
  ret void
}

attributes #0 = { "disable-tail-calls"="true" "frame-pointer"="all" }
