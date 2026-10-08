; REQUIRES: i8085
; RUN: split-file %s %t
; RUN: llvm-as %t/a.ll -o %t/a.o
; RUN: llvm-as %t/b.ll -o %t/b.o
; RUN: llvm-as %t/entry.ll -o %t/entry.o
; RUN: ld.lld -m i8085elf --lto-linker-scripts --gc-sections -T %t/script.ld %t/entry.o %t/a.o %t/b.o -o %t/full
; RUN: llvm-readelf -s %t/full | FileCheck %s --check-prefix=SYMS
; RUN: llvm-ar rcs %t/libparts.a %t/a.o %t/b.o
; RUN: ld.lld -m i8085elf --lto-linker-scripts --gc-sections -T %t/archive.ld %t/entry.o %t/libparts.a -o %t/archive
; RUN: llvm-readelf -s %t/archive | FileCheck %s --check-prefix=SYMS
; RUN: opt -module-summary %t/a.o -o %t/thin.o
; RUN: not ld.lld -m i8085elf --lto-linker-scripts %t/thin.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=THIN
; RUN: llvm-as %t/unwind.ll -o %t/unwind.o
; RUN: not ld.lld -m i8085elf --lto-linker-scripts -e unwind %t/unwind.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNWIND
; RUN: llvm-as %t/asm.ll -o %t/asm.o
; RUN: not ld.lld -m i8085elf --lto-linker-scripts -e asm %t/asm.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ASM
; RUN: not ld.lld -m i8085elf --lto-linker-scripts -e first -T %t/flags.ld %t/a.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=FLAGS

; SYMS-DAG: keep_fn
; SYMS-DAG: keep_data
; SYMS-DAG: first
; SYMS-DAG: second
; SYMS-DAG: shared
; SYMS-NOT: excluded
; THIN: --lto-linker-scripts requires full LTO
; UNWIND: --lto-linker-scripts: cannot attribute generated section .eh_frame
; ASM: --lto-linker-scripts: module asm, aliases and ifuncs are not supported
; FLAGS: --lto-linker-scripts: KEEP flags other than SHF_ALLOC/SHF_WRITE/SHF_EXECINSTR are not supported

;--- a.ll
target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
@local = internal global i16 1234, align 1
@keep_data = internal global i16 5678, section ".keep.data", align 1
$shared = comdat any
define internal i16 @helper() noinline nounwind optnone {
  %v = load volatile i16, ptr @local, align 1
  ret i16 %v
}
define i16 @first() noinline nounwind {
  %v = call i16 @helper()
  ret i16 %v
}
define internal void @keep_fn() noinline nounwind section ".keep.fn" { ret void }
define weak_odr i16 @shared() nounwind section ".shared" comdat { ret i16 111 }

;--- b.ll
target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
@local = internal global i16 4321, align 1
$shared = comdat any
define internal i16 @helper() noinline nounwind optnone {
  %v = load volatile i16, ptr @local, align 1
  ret i16 %v
}
define i16 @second() noinline nounwind {
  %v = call i16 @helper()
  ret i16 %v
}
define internal void @excluded() noinline nounwind section ".excluded.fn" { ret void }
define weak_odr i16 @shared() nounwind section ".shared" comdat { ret i16 111 }

;--- entry.ll
target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
declare i16 @first()
declare i16 @second()
declare i16 @shared()
define i16 @_start() nounwind {
  %a = call i16 @first()
  %b = call i16 @second()
  %c = call i16 @shared()
  %d = add i16 %a, %b
  %e = add i16 %d, %c
  ret i16 %e
}

;--- script.ld
ENTRY(_start)
SECTIONS {
  .boot 0x40 : { *entry.o(.text*) }
  .first 0x100 : { *a.o(.text* .data* .bss* .shared) }
  .second 0x300 : { *b.o(.text* .data* .bss* .shared) }
  .kept 0x500 : { KEEP(*a.o(.keep.*)) KEEP(*(EXCLUDE_FILE(*b.o) .excluded.*)) }
  /DISCARD/ : { *(.comment .note.*) }
}
ASSERT(first >= 0x100 && first < 0x300, "first lost its TU")
ASSERT(second >= 0x300 && second < 0x500, "second lost its TU")
ASSERT(shared >= 0x100 && shared < 0x300, "prevailing COMDAT lost its TU")
ASSERT(SIZEOF(.kept) == 3, "KEEP did not preserve local code and data")

;--- archive.ld
ENTRY(_start)
SECTIONS {
  .boot 0x40 : { *entry.o(.text*) }
  .first 0x100 : { *libparts.a:a.o(.text* .data* .bss* .shared) }
  .second 0x300 : { *libparts.a:b.o(.text* .data* .bss* .shared) }
  .kept 0x500 : { KEEP(*libparts.a:a.o(.keep.*)) KEEP(*(EXCLUDE_FILE(*libparts.a:b.o) .excluded.*)) }
  /DISCARD/ : { *(.comment .note.*) }
}
ASSERT(first >= 0x100 && first < 0x300, "archive first lost its TU")
ASSERT(second >= 0x300 && second < 0x500, "archive second lost its TU")
ASSERT(shared >= 0x100 && shared < 0x300, "archive COMDAT lost its TU")
ASSERT(SIZEOF(.kept) == 3, "archive KEEP failed")

;--- unwind.ll
target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
define void @unwind() uwtable { ret void }

;--- asm.ll
target datalayout = "e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8-a:8"
target triple = "i8085-unknown-elf"
module asm "nop"
define void @asm() nounwind { ret void }

;--- flags.ld
SECTIONS {
  .text : { KEEP(INPUT_SECTION_FLAGS(SHF_MERGE) *(.text*)) }
}
