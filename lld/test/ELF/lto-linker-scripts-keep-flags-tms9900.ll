; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: llc -filetype=obj main.ll -o native.o
; RUN: ld.lld --gc-sections -T script.ld native.o -o native
; RUN: llvm-objdump -s -j .kept -j .provided native | FileCheck %s --check-prefix=DATA
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T script.ld main.o -o out
; RUN: llvm-objdump -s -j .kept -j .provided out | FileCheck %s --check-prefix=DATA
; RUN: llvm-readobj --sections out | FileCheck %s --check-prefix=FLAGS
;
; Adding KEEP preservation must not manufacture SHF_GNU_RETAIN on an input
; section: that changes which INPUT_SECTION_FLAGS rule selects it later.
; A user-provided llvm.used must still retain its ordinary ELF meaning.
; DATA: Contents of section .kept:
; DATA-NEXT: 6000 3344
; DATA: Contents of section .provided:
; DATA-NEXT: 6100 1122
; FLAGS: Name: .kept
; FLAGS: Flags [ (0x3)
; FLAGS: Name: .provided
; FLAGS: Flags [ (0x200003)

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@provided = internal global i16 4386, section ".payload", align 2
@kept = internal global i16 13124, section ".payload", align 2
@llvm.used = appending global [1 x ptr] [ptr @provided], section "llvm.metadata"
; Extending this list replaces its GlobalVariable. IRMover must not retain
; the stale pointer collected before the append.
@llvm.compiler.used = appending global [1 x ptr] [ptr @kept], section "llvm.metadata"
define void @_start() { ret void }

;--- script.ld
ENTRY(_start)
SECTIONS {
  /* 0x200000 is SHF_GNU_RETAIN (numeric flag masks are supported too). */
  .kept 0x6000 : { KEEP(INPUT_SECTION_FLAGS(!0x200000) *(.payload)) }
  .provided 0x6100 : { INPUT_SECTION_FLAGS(0x200000) *(.payload) }
  .text 0x6200 : { *(.text*) }
}
