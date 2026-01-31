; RUN: llc -mtriple=i8085-unknown-elf -O0 -stop-after=finalize-isel -o - %s | FileCheck %s

define i64 @ret_i64_sext(i16 %x) {
  %ext = sext i16 %x to i64
  ret i64 %ext
}

; CHECK-LABEL: name: ret_i64_sext
; CHECK-NOT: SRL
; CHECK: RET{{.*}}$iax{{.*}}$ibx
