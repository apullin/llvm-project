; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: llc -filetype=obj main.ll -o native.o
; RUN: ld.lld -T script.ld native.o -o native
; RUN: llvm-readobj --sections --symbols native | FileCheck %s --check-prefix=FINAL --implicit-check-not="Name: .grouped"
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T script.ld main.o -o out
; RUN: llvm-dis out.0.0.preopt.bc -o - | FileCheck %s --check-prefix=IR
; RUN: llvm-readobj --sections --symbols out | FileCheck %s --check-prefix=FINAL --implicit-check-not="Name: .grouped"
; RUN: ld.lld --lto-linker-scripts --lto-Oz -T script.ld main.o -o size
; RUN: llvm-readobj --sections --symbols size | FileCheck %s --check-prefix=FINAL --implicit-check-not="Name: .grouped"
; RUN: ld.lld -r -T script.ld native.o -o native-relocatable
; RUN: llvm-readobj --sections --symbols native-relocatable | FileCheck %s --check-prefix=RELOC
; RUN: ld.lld -r --lto-linker-scripts --lto-O2 --save-temps -T script.ld main.o -o relocatable
; RUN: llvm-dis relocatable.0.0.preopt.bc -o - | FileCheck %s --check-prefix=RELOC-IR
; RUN: llvm-readobj --sections --symbols relocatable | FileCheck %s --check-prefix=RELOC
; RUN: ld.lld -r --force-group-allocation -T script.ld native.o -o native-forced
; RUN: llvm-readobj --sections --symbols native-forced | FileCheck %s --check-prefix=FORCED --implicit-check-not="Name: .grouped"
; RUN: ld.lld -r --force-group-allocation --lto-linker-scripts --lto-O2 --save-temps -T script.ld main.o -o forced
; RUN: llvm-dis forced.0.0.preopt.bc -o - | FileCheck %s --check-prefix=IR
; RUN: llvm-readobj --sections --symbols forced | FileCheck %s --check-prefix=FORCED --implicit-check-not="Name: .grouped"
;
; Native inputs lose SHF_GROUP before script matching in a final link; the
; pre-matcher must do the same. Relocatable links retain the group by default.
; IR: define {{.*}} @helper() #[[HELPER:[0-9]+]]
; IR: attributes #[[HELPER]] = {{.*}}"linker_output_section"="script:2;"
; FINAL: Name: .text
; FINAL: Address: 0x6200
; FINAL: Name: helper
; FINAL: Section: .text
; RELOC-IR: define {{.*}} @helper() #[[HELPER:[0-9]+]]
; RELOC-IR: attributes #[[HELPER]] = {{.*}}"linker_output_section"="script:0;"
; RELOC: Name: .grouped
; RELOC: SHF_GROUP
; RELOC: Name: helper
; RELOC: Section: .grouped
; FORCED: Name: .text
; FORCED: Name: helper
; FORCED: Section: .text

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
$helper = comdat any
@sink = global i16 0, section ".bss", align 2
define linkonce_odr i16 @helper() section ".text.helper" comdat {
  %x = load volatile i16, ptr @sink
  ret i16 %x
}
define i16 @_start() section ".text.start" {
  %x = call i16 @helper()
  ret i16 %x
}

;--- script.ld
ENTRY(_start)
SECTIONS {
  .grouped 0x6000 : { INPUT_SECTION_FLAGS(SHF_GROUP) *(.text.helper) }
  .start 0x6100 : { *(.text.start) }
  .text 0x6200 : { *(.text*) }
  .bss 0xa000 : { *(.bss) }
}
