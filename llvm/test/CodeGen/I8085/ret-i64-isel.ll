; RUN: llc -mtriple=i8085-unknown-elf -O0 -stop-after=finalize-isel -o - %s | FileCheck %s

define i64 @ret_i64_sext(i16 %x) {
  %ext = sext i16 %x to i64
  ret i64 %ext
}

; CHECK-LABEL: name: ret_i64_sext
; CHECK-NOT: SRL
; CHECK: %3:gr16bd = COPY %1
; CHECK: %2:gr32 = SEXT16TO32 %3
; CHECK: %4:gr32 = ASR_32_BY_16 %2
; CHECK: STORE_32_ADDR_CONTENT %0, %2
; CHECK: STORE_32 %20, 4, killed %19
; CHECK: RET
