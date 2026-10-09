; RUN: llc -mtriple=i8085-unknown-elf -O2 -verify-machineinstrs -stop-after=i8085-addr-hints -o - %s | FileCheck %s
; RUN: llc -mtriple=i8085-unknown-elf -O2 -verify-machineinstrs -o /dev/null %s

define i16 @single_use(ptr %p) #0 {
  %value = load i16, ptr %p
  ret i16 %value
}

; CHECK-LABEL: name: single_use
; CHECK-NOT: LOAD_16_ADDR_CONTENT_SCRATCH
; CHECK: {{%[0-9]+}}:gr16 = LOAD_16_ADDR_CONTENT {{(killed )?}}%{{[0-9]+}}, implicit-def {{(dead )?}}$a
; CHECK-NOT: LOAD_16_ADDR_CONTENT_SCRATCH

define i16 @read_modify_write(ptr %p, i16 %delta) #0 {
  %old = load i16, ptr %p
  %new = add i16 %old, %delta
  store i16 %new, ptr %p
  ret i16 %old
}

; CHECK-LABEL: name: read_modify_write
; CHECK: {{%[0-9]+}}:gr16bd = LOAD_16_ADDR_CONTENT_SCRATCH %{{[0-9]+}}{{.*}}implicit-def $hl
; CHECK: STORE_16_ADDR_CONTENT

define i16 @swap(ptr %p, ptr %q) #0 {
  %a = load i16, ptr %p
  %b = load i16, ptr %q
  store i16 %b, ptr %p
  store i16 %a, ptr %q
  %difference = sub i16 %a, %b
  ret i16 %difference
}

; CHECK-LABEL: name: swap
; CHECK: {{%[0-9]+}}:gr16bd = LOAD_16_ADDR_CONTENT_SCRATCH %{{[0-9]+}}{{.*}}implicit-def $hl
; CHECK: {{%[0-9]+}}:gr16bd = LOAD_16_ADDR_CONTENT_SCRATCH %{{[0-9]+}}{{.*}}implicit-def $hl
; CHECK: SUB_16

define i16 @read_only(ptr %p) #0 {
  %a = load volatile i16, ptr %p
  %b = load volatile i16, ptr %p
  %sum = add i16 %a, %b
  ret i16 %sum
}

; CHECK-LABEL: name: read_only
; CHECK-NOT: LOAD_16_ADDR_CONTENT_SCRATCH
; CHECK-COUNT-2: :gr16 = LOAD_16_ADDR_CONTENT
; CHECK-NOT: LOAD_16_ADDR_CONTENT_SCRATCH

define i16 @conditional_store(ptr %p, i16 %delta, i1 %write) #0 {
  %old = load i16, ptr %p
  br i1 %write, label %store, label %exit
store:
  %new = add i16 %old, %delta
  store i16 %new, ptr %p
  br label %exit
exit:
  ret i16 %old
}

; CHECK-LABEL: name: conditional_store
; CHECK: {{%[0-9]+}}:gr16bd = LOAD_16_ADDR_CONTENT_SCRATCH %{{[0-9]+}}{{.*}}implicit-def $hl
; CHECK: STORE_16_ADDR_CONTENT

define i16 @speed_optimized(ptr %p, i16 %delta) {
  %old = load i16, ptr %p
  %new = add i16 %old, %delta
  store i16 %new, ptr %p
  ret i16 %old
}

; CHECK-LABEL: name: speed_optimized
; CHECK-NOT: LOAD_16_ADDR_CONTENT_SCRATCH
; CHECK: {{%[0-9]+}}:gr16 = LOAD_16_ADDR_CONTENT
; CHECK-NOT: LOAD_16_ADDR_CONTENT_SCRATCH

attributes #0 = { optsize }
