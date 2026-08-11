; RUN: llc -mtriple=tms9900 -tms9900-asm-dialect=xas99 < %s \
; RUN:   | FileCheck --check-prefix=ASM %s
; RUN: llc -mtriple=tms9900 -tms9900-asm-dialect=xas99 < %s -o %t.s
; RUN: llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %t.s -o %t.o
; RUN: llvm-readobj --hex-dump=.rodata %t.o | FileCheck --check-prefix=OBJ %s

@str = constant [4 x i8] c"A\0AB\00"

; ASM:     TEXT "A\nB\000"
; ASM-NOT: TEXT '"
; OBJ:     0x00000000 410a4200
