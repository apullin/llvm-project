; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: ld.lld --lto-linker-scripts --lto-O0 --save-temps -T script.ld main.o -o unoptimized
; RUN: llvm-dis unoptimized.0.4.opt.bc -o - | FileCheck %s
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T script.ld main.o -o optimized
; RUN: llvm-dis optimized.0.4.opt.bc -o - | FileCheck %s
; RUN: ld.lld --lto-linker-scripts --lto-Oz --save-temps -T script.ld main.o -o size
; RUN: llvm-dis size.0.4.opt.bc -o - | FileCheck %s
;
; Mandatory inlining still has to respect placement. Full-LTO -O0 uses a
; separate always-inliner, and the cost-based inliner must check placement
; before accepting an alwaysinline attribute too.
; CHECK-LABEL: define {{.*}} @_start(
; CHECK: call {{.*}}i16 @helper(

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@sink = global i16 0, section ".bss", align 2
define i16 @helper() alwaysinline section ".text.helper" {
  %x = load volatile i16, ptr @sink
  ret i16 %x
}
define i16 @_start() section ".text.start" {
  %r = call i16 @helper()
  ret i16 %r
}

;--- script.ld
ENTRY(_start)
SECTIONS {
  .helper 0x6000 : { *(.text.helper) }
  .start 0x6100 : { *(.text.start) }
  .bss 0xa000 : { *(.bss) }
}
