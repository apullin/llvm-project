; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as a.ll -o a.o
; RUN: llvm-as b.ll -o b.o
; RUN: ld.lld --lto-linker-scripts --lto-O0 -T script.ld a.o b.o -o out
; RUN: llvm-objdump -s -j .a -j .b out | FileCheck %s
; RUN: ld.lld --lto-linker-scripts --lto-O0 -T script.ld b.o a.o -o reverse
; RUN: llvm-objdump -s -j .a -j .b reverse | FileCheck %s
; RUN: llvm-ar rc libb.a b.o
; RUN: ld.lld --lto-linker-scripts --lto-O0 --undefined=collision -T archive.ld a.o libb.a -o archive-out
; RUN: llvm-objdump -s -j .a -j .b archive-out | FileCheck %s
; RUN: opt -module-summary a.ll -o thin-a.o
; RUN: opt -module-summary b.ll -o thin-b.o
; RUN: ld.lld --lto-linker-scripts --lto-O0 --thinlto-jobs=1 -T thin.ld thin-a.o thin-b.o -o thin-out
; RUN: llvm-objdump -s -j .values thin-out | FileCheck %s --check-prefix=THIN
; RUN: llvm-as archive-main.ll -o archive-main.o
; RUN: llvm-as archive-local.ll -o archive-local.o
; RUN: llvm-ar rc liblocal.a archive-local.o
; RUN: not ld.lld --lto-linker-scripts --lto-O0 -Ttext=0 archive-main.o liblocal.a -o bad 2>&1 | FileCheck %s --check-prefix=UNDEF
; RUN: not ld.lld --lto-linker-scripts --lto-O0 -Ttext=0 archive-main.o archive-local.o -o bad 2>&1 | FileCheck %s --check-prefix=UNDEF
;
; File-local definitions must not participate in external name resolution.
; Both files have a static "shared"; a static "collision" also shares its
; name with b.o's externally visible definition. KEEP must retain each copy
; and preserve translation-unit input matching in either command-line order.

; CHECK: Contents of section .a:
; CHECK-NEXT: 6000 00110033
; CHECK: Contents of section .b:
; CHECK-NEXT: 6100 00220044
; THIN: Contents of section .values:
; THIN-NEXT: 6000 00110033 00220044
; UNDEF: undefined symbol: only_local

;--- a.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@shared = internal global i16 17, section ".data.shared", align 2
@collision = internal global i16 51, section ".data.collision", align 2
declare i16 @use_b()
define i16 @_start() section ".text.start" {
  %a = load volatile i16, ptr @shared
  %b = load volatile i16, ptr @collision
  %c = call i16 @use_b()
  %sum = add i16 %a, %b
  %result = add i16 %sum, %c
  ret i16 %result
}

;--- b.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@shared = internal global i16 34, section ".data.shared", align 2
@collision = global i16 68, section ".data.collision", align 2
define i16 @use_b() section ".text.use" {
  %a = load volatile i16, ptr @shared
  %b = load volatile i16, ptr @collision
  %result = add i16 %a, %b
  ret i16 %result
}

;--- script.ld
SECTIONS {
  .a 0x6000 : { KEEP(a.o(.data.*)) }
  .b 0x6100 : { KEEP(b.o(.data.*)) }
  .text 0x6200 : { *(.text*) }
}

;--- archive-main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
declare i16 @only_local()
define i16 @_start() {
  %result = call i16 @only_local()
  ret i16 %result
}

;--- archive-local.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
define internal i16 @only_local() section ".text.only_local" {
  ret i16 42
}
define i16 @anchor() {
  %result = call i16 @only_local()
  ret i16 %result
}

;--- archive.ld
SECTIONS {
  .a 0x6000 : { KEEP(a.o(.data.*)) }
  .b 0x6100 : { KEEP(libb.a:b.o(.data.*)) }
  .text 0x6200 : { *(.text*) }
}

;--- thin.ld
SECTIONS {
  .values 0x6000 : { *(.data.*) }
  .text 0x6200 : { *(.text*) }
}
