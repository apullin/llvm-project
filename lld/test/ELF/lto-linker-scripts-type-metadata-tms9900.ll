; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: ld.lld --lto-linker-scripts -m elf32-tms9900 -T script.ld main.o -o out
; RUN: llvm-readobj --sections --symbols out | FileCheck %s

; CHECK:      Name: .rodata
; CHECK:      Address: 0x6100
; CHECK:      Name: vtable
; CHECK-NEXT: Value:
; CHECK-NEXT: Size:
; CHECK-NEXT: Binding: Global
; CHECK-NEXT: Type: None
; CHECK-NEXT: Other:
; CHECK-NEXT: Section: .rodata

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"

@vtable = constant [1 x ptr] [ptr @_start], section ".rodata.vtable", !type !0
@llvm.used = appending global [1 x ptr] [ptr @vtable], section "llvm.metadata"

define i16 @_start() section ".text" {
entry:
  ret i16 0
}

!0 = !{i16 0, !"vtable.type"}

;--- script.ld
SECTIONS {
  .text 0x6000 : { *(.text*) }
  .rodata 0x6100 : { *(.rodata*) }
}
