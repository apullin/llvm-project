; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as readonly.ll -o readonly.o
; RUN: llvm-as writable.ll -o writable.o
; RUN: llc -filetype=obj readonly.ll -o readonly-native.o
; RUN: llc -filetype=obj writable.ll -o writable-native.o
; RUN: ld.lld --gc-sections -T script.ld readonly-native.o writable-native.o -o native
; RUN: llvm-objdump -s -j .selected native | FileCheck %s --check-prefix=DATA
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T script.ld readonly.o writable.o -o out
; RUN: llvm-objdump -s -j .selected out | FileCheck %s --check-prefix=DATA
; RUN: llvm-readobj --sections out | FileCheck %s --check-prefix=METADATA
; RUN: llvm-nm out | FileCheck %s --check-prefix=NAMES --implicit-check-not=readonly_value
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T script.ld writable.o readonly.o -o reverse
; RUN: llvm-nm reverse | FileCheck %s --check-prefix=NAMES --implicit-check-not=readonly_value
; RUN: llvm-ar rc libvalues.a writable.o
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T script.ld readonly.o --whole-archive libvalues.a -o archive
; RUN: llvm-objdump -s -j .selected archive | FileCheck %s --check-prefix=DATA
; RUN: llvm-nm archive | FileCheck %s --check-prefix=NAMES --implicit-check-not=readonly_value
;
; KEEP must apply its INPUT_SECTION_FLAGS predicate before LTO preservation,
; even when unrelated writable and readonly globals use the same section name.
; DATA: Contents of section .selected:
; DATA-NEXT: 6000 {{33440000|00003344}}
; NAMES: writable_value
; METADATA: Name: .metadata
; METADATA: Flags [ (0x0)
; METADATA-NEXT: ]

;--- readonly.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@readonly_value = internal constant i16 4386, section ".payload", align 2
; Name-based BSS lowering is writable even when the IR global is constant.
@forced_bss = internal constant i16 0, section ".bss", align 2
; This special name has no ALLOC bit. Both queries and suffixed codegen must
; preserve that, rather than deriving flags from IR constness alone.
@metadata_value = internal constant i16 21862, section ".llvmcmd", align 2
define void @_start() { ret void }

;--- writable.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@writable_value = internal global i16 13124, section ".payload", align 2

;--- script.ld
ENTRY(_start)
SECTIONS {
  .selected 0x6000 : { KEEP(INPUT_SECTION_FLAGS(SHF_WRITE & !SHF_EXECINSTR) *(.payload .bss)) }
  .metadata : { KEEP(INPUT_SECTION_FLAGS(!SHF_ALLOC) *(.llvmcmd)) }
  .text 0x6100 : { *(.text*) }
}
