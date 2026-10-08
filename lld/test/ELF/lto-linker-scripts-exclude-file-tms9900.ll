; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as included.ll -o included.o
; RUN: llvm-as excluded.ll -o excluded.o
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T script.ld included.o excluded.o -o out
; RUN: llvm-objdump -s -j .kept out | FileCheck %s --check-prefix=DATA
; RUN: llvm-nm out | FileCheck %s --check-prefix=NAMES --implicit-check-not=excluded_value
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T script.ld excluded.o included.o -o reverse
; RUN: llvm-nm reverse | FileCheck %s --check-prefix=NAMES --implicit-check-not=excluded_value
; RUN: llvm-ar rc libexcluded.a excluded.o
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections --undefined=anchor -T archive.ld included.o libexcluded.a -o archive-out
; RUN: llvm-objdump -s -j .kept archive-out | FileCheck %s --check-prefix=DATA
; RUN: llvm-nm archive-out | FileCheck %s --check-prefix=NAMES --implicit-check-not=excluded_value
;
; An unused local matched by KEEP must survive unless its original input file
; is excluded. EXCLUDE_FILE must apply before LTO's llvm.used preservation.
;
; DATA: Contents of section .kept:
; DATA-NEXT: 6000 1122
; NAMES: included_value

;--- included.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@included_value = internal global i16 4386, section ".data.keep", align 2
define void @_start() { ret void }

;--- excluded.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@excluded_value = internal global i16 13124, section ".data.keep", align 2
define void @anchor() { ret void }

;--- script.ld
ENTRY(_start)
SECTIONS {
  .kept 0x6000 : { KEEP(*(EXCLUDE_FILE(excluded.o) .data.keep)) }
  .text 0x6100 : { *(.text*) }
}

;--- archive.ld
ENTRY(_start)
SECTIONS {
  .kept 0x6000 : { KEEP(*(EXCLUDE_FILE(libexcluded.a:excluded.o) .data.keep)) }
  .text 0x6100 : { *(.text*) }
}
