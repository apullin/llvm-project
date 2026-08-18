; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: ld.lld -m elf32-tms9900 -T script.ld -e outer -u outer2 --lto-O2 --save-temps main.o -o o2
; RUN: llvm-dis < o2.0.4.opt.bc | FileCheck %s --check-prefix=O2
; RUN: ld.lld -m elf32-tms9900 -T script.ld -e outer -u outer2 --lto-Os --save-temps main.o -o os
; RUN: llvm-dis < os.0.4.opt.bc | FileCheck %s --check-prefix=OS
; RUN: ld.lld -m elf32-tms9900 -T script.ld -e outer -u outer2 --plugin-opt=Oz --save-temps main.o -o oz
; RUN: llvm-dis < oz.0.4.opt.bc | FileCheck %s --check-prefix=OZ

; O2-LABEL: define dso_local i16 @outer
; O2-NOT: call i16 @inner
; O2: ret i16

; OS-LABEL: define dso_local i16 @outer
; OS-NOT: call i16 @inner
; OS: ret i16

; OZ-LABEL: define dso_local i16 @outer
; OZ: call fastcc i16 @inner

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"

@a = global i16 4, align 2

define internal void @external_work() noinline {
entry:
  store volatile i16 4, ptr @a, align 2
  ret void
}

define internal i16 @inner() {
entry:
  call void @external_work()
  %a1 = load volatile i16, ptr @a, align 2
  %x1 = add i16 %a1, %a1
  %a2 = load volatile i16, ptr @a, align 2
  %x2 = add i16 %x1, %a2
  %a3 = load volatile i16, ptr @a, align 2
  %x3 = add i16 %x2, %a3
  %a4 = load volatile i16, ptr @a, align 2
  %x4 = add i16 %x3, %a4
  %a5 = load volatile i16, ptr @a, align 2
  %x5 = add i16 %x4, %a5
  %a6 = load volatile i16, ptr @a, align 2
  %x6 = add i16 %x5, %a6
  %a7 = load volatile i16, ptr @a, align 2
  %x7 = add i16 %x6, %a7
  %a8 = load volatile i16, ptr @a, align 2
  %x8 = add i16 %x7, %a8
  %a9 = load volatile i16, ptr @a, align 2
  %x9 = add i16 %x8, %a9
  %a10 = load volatile i16, ptr @a, align 2
  %x10 = add i16 %x9, %a10
  ret i16 %x10
}

define i16 @outer() optsize {
entry:
  %r = call i16 @inner()
  ret i16 %r
}

define i16 @outer2() minsize {
entry:
  %r = call i16 @inner()
  ret i16 %r
}

;--- script.ld
SECTIONS {
  .text 0x6000 : { *(.text*) }
  .data 0x7000 : { *(.data*) }
}
