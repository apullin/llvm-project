; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: ld.lld --lto-linker-scripts -m elf32-tms9900 -T script.ld --save-temps main.o -o out
; RUN: FileCheck %s --check-prefix=RES < out.resolution.txt
; RUN: llvm-dis < out.0.4.opt.bc | FileCheck %s --check-prefix=OPT
; RUN: llvm-readobj --sections --symbols out | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d out | FileCheck %s --check-prefix=DISASM

; RES: -r=main.o,keepme,plxk

; OPT: @llvm.used = appending global {{.*}} @keepme
; OPT: define internal {{.*}} @fast{{.*}} section ".text.fast^^main.o"
; OPT: define internal {{.*}} @keepme{{.*}} section ".text.keepme^^main.o"
; OPT: define dso_local {{.*}} @_start{{.*}} section ".text^^main.o"
; OPT: call fastcc i16 @fast
; OPT: "linker_output_section"=".fast"
; OPT: "linker_output_section"=".keep"
; OPT: "linker_output_section"=".text"

; ELF:      Name: .fast
; ELF:      Address: 0x6000
; ELF:      Name: .keep
; ELF:      SHF_GNU_RETAIN
; ELF:      Address: 0x6100
; ELF:      Name: .text
; ELF:      Address: 0x6200
; ELF:      Name: .bss
; ELF:      Address: 0x7000
; ELF:      Name: fast
; ELF:      Section: .fast
; ELF:      Name: keepme
; ELF:      Section: .keep
; ELF:      Name: _start
; ELF:      Section: .text

; DISASM: Disassembly of section .fast:
; DISASM: <fast>:
; DISASM: Disassembly of section .keep:
; DISASM: <keepme>:
; DISASM: Disassembly of section .text:
; DISASM: <_start>:

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"

@sink = global i16 0, section ".bss", align 2

define i16 @fast(i16 %x) section ".text.fast" {
entry:
  store volatile i16 %x, ptr @sink, align 2
  %v = load volatile i16, ptr @sink, align 2
  %r = add i16 %v, 1
  ret i16 %r
}

define internal i16 @keepme() section ".text.keepme" {
entry:
  store volatile i16 7, ptr @sink, align 2
  %v = load volatile i16, ptr @sink, align 2
  ret i16 %v
}

define i16 @_start() section ".text" {
entry:
  %x = load volatile i16, ptr @sink, align 2
  %r = call i16 @fast(i16 %x)
  ret i16 %r
}

;--- script.ld
SECTIONS {
  .fast 0x6000 : { *(.text.fast*) }
  .keep 0x6100 : { KEEP(*(.text.keepme*)) }
  .text 0x6200 : { *(.text*) }
  .bss 0x7000 : { *(.bss*) }
}
