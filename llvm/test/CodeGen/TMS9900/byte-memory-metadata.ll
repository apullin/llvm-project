; RUN: llc -mtriple=tms9900 -stop-after=finalize-isel -verify-machineinstrs < %s | FileCheck %s

; Custom byte lowering must preserve the original MachineMemOperand. In
; particular, losing the volatile flag would let later machine passes treat
; memory-mapped I/O accesses as ordinary memory operations.

define i8 @volatile_load_byte(ptr %p) {
; CHECK-LABEL: name: volatile_load_byte
; CHECK: MOVBim {{.*}} :: (volatile load (s8)
  %value = load volatile i8, ptr %p, align 1
  ret i8 %value
}

define void @volatile_store_byte(ptr %p, i8 %value) {
; CHECK-LABEL: name: volatile_store_byte
; CHECK: MOVBmi {{.*}} :: (volatile store (s8)
  store volatile i8 %value, ptr %p, align 1
  ret void
}
