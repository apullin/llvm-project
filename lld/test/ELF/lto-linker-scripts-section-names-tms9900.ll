; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as first.ll -o 'odd^^source.o'
; RUN: llvm-as second.ll -o 'odd%5E%5Esource.o'
; RUN: llvm-mc -triple=tms9900 -filetype=obj native.s -o native.o
; RUN: ld.lld --lto-linker-scripts --gc-sections -T script.ld 'odd^^source.o' 'odd%5E%5Esource.o' native.o -o out
; RUN: llvm-objdump -s -j .first -j .second -j .native out | FileCheck %s
;
; Original section names can themselves contain the transport separator.
; TU identifiers are escaped bijectively, and a native object's literal name
; must not be interpreted as transport even when its suffix names an LTO TU.
; CHECK: Contents of section .first:
; CHECK-NEXT: 6000 1122
; CHECK: Contents of section .second:
; CHECK-NEXT: 6100 3344
; CHECK: Contents of section .native:
; CHECK-NEXT: 6200 5566

;--- first.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@first = internal constant i16 4386, section ".user^^section", align 2
define void @_start() { ret void }

;--- second.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@second = internal constant i16 13124, section ".user^^section", align 2

;--- native.s
.section ".literal^^odd%5E%5Esource.o","a",@progbits
.globl native_value
native_value:
  .short 0x5566

;--- script.ld
ENTRY(_start)
SECTIONS {
  .first 0x6000 : { KEEP("odd^^source.o"(.user^^section)) }
  .second 0x6100 : { KEEP("odd%5E%5Esource.o"(.user^^section)) }
  .native 0x6200 : { KEEP(native.o(".literal^^odd%5E%5Esource.o")) }
  .text 0x6300 : { *(.text*) }
}
