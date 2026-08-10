; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; The TMS9900 has no instruction that can implement an interrupt-safe atomic
; read/modify/write sequence.  Preserve the atomic contract by using LLVM's
; standard runtime ABI rather than silently emitting ordinary memory accesses.

@byte = external global i8, align 1
@word = external global i16, align 2

; CHECK-LABEL: load_word:
; CHECK: BL{{[ \t]+}}@__atomic_load_2
define i16 @load_word() {
  %value = load atomic i16, ptr @word seq_cst, align 2
  ret i16 %value
}

; CHECK-LABEL: store_word:
; CHECK: BL{{[ \t]+}}@__atomic_store_2
define void @store_word(i16 %value) {
  store atomic i16 %value, ptr @word seq_cst, align 2
  ret void
}

; CHECK-LABEL: add_word:
; CHECK: BL{{[ \t]+}}@__atomic_fetch_add_2
define i16 @add_word(i16 %increment) {
  %old = atomicrmw add ptr @word, i16 %increment seq_cst, align 2
  ret i16 %old
}

; CHECK-LABEL: exchange_byte:
; CHECK: BL{{[ \t]+}}@__atomic_exchange_1
define i8 @exchange_byte(i8 %value) {
  %old = atomicrmw xchg ptr @byte, i8 %value seq_cst, align 1
  ret i8 %old
}

; CHECK-LABEL: compare_word:
; CHECK: BL{{[ \t]+}}@__atomic_compare_exchange_2
define { i16, i1 } @compare_word(i16 %expected, i16 %desired) {
  %result = cmpxchg ptr @word, i16 %expected, i16 %desired seq_cst seq_cst, align 2
  ret { i16, i1 } %result
}
