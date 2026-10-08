; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: opt -module-summary main.ll -o thin.o
; RUN: llvm-as main.ll -o full.o
; RUN: ld.lld --lto-linker-scripts --gc-sections -T script.ld full.o -o full
; RUN: llvm-objdump -s -j .kept full | FileCheck %s --check-prefix=FULL
; RUN: not ld.lld --lto-linker-scripts --gc-sections -T script.ld thin.o -o bad 2>&1 | FileCheck %s --check-prefix=ERROR
; RUN: not ld.lld --lto-linker-scripts --gc-sections -T script.ld full.o thin.o -o mixed 2>&1 | FileCheck %s --check-prefix=ERROR
; RUN: not ld.lld --lto-linker-scripts --thinlto-cache-dir=cache --gc-sections -T script.ld thin.o -o cached 2>&1 | FileCheck %s --check-prefix=ERROR
; RUN: llvm-ar rc libthin.a thin.o
; RUN: not ld.lld --lto-linker-scripts --gc-sections -T script.ld --whole-archive libthin.a -o archive 2>&1 | FileCheck %s --check-prefix=ARCHIVE
;
; ThinLTO has no transport for the full-LTO placement keys, KEEP preservation,
; or cache invalidation. Do not silently succeed while dropping required data.
; FULL: Contents of section .kept:
; FULL-NEXT: 6000 1122
; ERROR: linker-script-aware LTO requires full LTO; recompile 'thin.o' with -flto=full
; ARCHIVE: linker-script-aware LTO requires full LTO; recompile 'libthin.a(thin.o at {{[0-9]+}})' with -flto=full

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@value = internal constant i16 4386, section ".payload", align 2
define weak void @_start() { ret void }

;--- script.ld
ENTRY(_start)
SECTIONS {
  .kept 0x6000 : { KEEP(*(.payload)) }
  .text 0x6100 : { *(.text*) }
}
