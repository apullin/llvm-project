; REQUIRES: tms9900
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-as main.ll -o main.o
; RUN: llc -filetype=obj main.ll -o native.o
; RUN: ld.lld -T ro.ld native.o -o native-ro
; RUN: llvm-readobj --sections native-ro | FileCheck %s --check-prefix=SPLIT --implicit-check-not='.rejected'
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T ro.ld main.o -o ro
; RUN: llvm-dis ro.0.4.opt.bc -o - | FileCheck %s --check-prefix=CALL
; RUN: llvm-readobj --sections ro | FileCheck %s --check-prefix=SPLIT --implicit-check-not='.rejected'
; RUN: ld.lld --lto-linker-scripts --lto-Oz --save-temps -T ro.ld main.o -o ro-size
; RUN: llvm-dis ro-size.0.4.opt.bc -o - | FileCheck %s --check-prefix=CALL
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T rw.ld main.o -o rw
; RUN: llvm-dis rw.0.4.opt.bc -o - | FileCheck %s --check-prefix=CALL
; RUN: llvm-readobj --sections rw | FileCheck %s --check-prefix=SPLIT --implicit-check-not='.rejected'
; RUN: ld.lld -T duplicate.ld native.o -o native-duplicate
; RUN: llvm-readobj --sections native-duplicate | FileCheck %s --check-prefix=DUPLICATE
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T duplicate.ld main.o -o duplicate
; RUN: llvm-dis duplicate.0.4.opt.bc -o - | FileCheck %s --check-prefix=CALL
; RUN: llvm-readobj --sections duplicate | FileCheck %s --check-prefix=DUPLICATE
; RUN: ld.lld -T orphan.ld native.o -o native-orphan
; RUN: llvm-readobj --sections native-orphan | FileCheck %s --check-prefix=ORPHAN
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T orphan.ld main.o -o orphan
; RUN: llvm-dis orphan.0.4.opt.bc -o - | FileCheck %s --check-prefix=CALL
; RUN: llvm-readobj --sections orphan | FileCheck %s --check-prefix=ORPHAN --implicit-check-not='^^'
; RUN: ld.lld --lto-linker-scripts --lto-O2 --save-temps -T shared.ld main.o -o shared
; RUN: llvm-dis shared.0.4.opt.bc -o - | FileCheck %s --check-prefix=INLINE
; RUN: ld.lld -T mixed.ld native.o -o native-mixed
; RUN: llvm-readobj --sections native-mixed | FileCheck %s --check-prefix=MIXED
; RUN: ld.lld --lto-linker-scripts --lto-O2 -T mixed.ld main.o -o mixed
; RUN: llvm-readobj --sections mixed | FileCheck %s --check-prefix=MIXED
; RUN: ld.lld --gc-sections -T gc.ld native.o -o native-gc
; RUN: llvm-readobj --sections native-gc | FileCheck %s --check-prefix=GC
; RUN: ld.lld --lto-linker-scripts --lto-O2 --gc-sections -T gc.ld main.o -o gc
; RUN: llvm-readobj --sections gc | FileCheck %s --check-prefix=GC
;
; A conditional output's final eligibility depends on its complete set of live
; native inputs. Rejecting it can send two functions to distinct fallbacks.
; Their placement partitions must therefore distinguish the complete paths,
; rather than assuming the first syntactic match will be used.
; CALL-LABEL: define {{.*}} @_start(
; CALL: call {{.*}}i16 @helper(
; INLINE-LABEL: define {{.*}} @_start(
; INLINE-NOT: call
; INLINE: load volatile i16, ptr @sink
; INLINE: ret i16
; SPLIT: Name: .caller
; SPLIT: Address: 0x6100
; SPLIT: Name: .callee
; SPLIT: Address: 0x6200
; DUPLICATE: Name: .bank
; DUPLICATE: Address: 0x6100
; DUPLICATE: Name: .bank
; DUPLICATE: Address: 0x6200
; ORPHAN-DAG: Name: .text.a
; ORPHAN-DAG: Name: .text.b
; MIXED: Name: .accepted
; MIXED: SHF_WRITE
; MIXED: Address: 0x6000
; GC: Name: .accepted
; GC: Address: 0x6000

;--- main.ll
target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-n16-S32"
target triple = "tms9900"
@payload = internal global i16 17, section ".payload", align 2
@sink = global i16 0, section ".bss", align 2
define i16 @helper() section ".text.b" {
  %x = load volatile i16, ptr @sink
  %r = add i16 %x, 1
  ret i16 %r
}
define i16 @_start() section ".text.a" {
  %r = call i16 @helper()
  ret i16 %r
}

;--- ro.ld
ENTRY(_start)
SECTIONS {
  .rejected 0x6000 : ONLY_IF_RO { KEEP(*(.text.a .text.b .payload)) }
  .caller 0x6100 : { *(.text.a) }
  .callee 0x6200 : { *(.text.b) }
  .data 0xa000 : { *(.payload .bss) }
}

;--- rw.ld
ENTRY(_start)
SECTIONS {
  .rejected 0x6000 : ONLY_IF_RW { KEEP(*(.text.a .text.b)) }
  .caller 0x6100 : { *(.text.a) }
  .callee 0x6200 : { *(.text.b) }
  .data 0xa000 : { *(.payload .bss) }
}

;--- duplicate.ld
ENTRY(_start)
SECTIONS {
  .bank 0x6100 : { KEEP(*(.text.a)) }
  .bank 0x6200 : { KEEP(*(.text.b)) }
  .data 0xa000 : { *(.payload .bss) }
}

;--- orphan.ld
ENTRY(_start)
SECTIONS {
  .data 0xa000 : { *(.payload .bss) }
}

;--- shared.ld
ENTRY(_start)
SECTIONS {
  .shared 0x6000 : { *(.text.a .text.b) }
  .data 0xa000 : { *(.payload .bss) }
}

;--- mixed.ld
ENTRY(_start)
SECTIONS {
  /* ONLY_IF_RW permits a mix of code and writable data. */
  .accepted 0x6000 : ONLY_IF_RW { KEEP(*(.text.a .text.b .payload)) }
  .caller 0x6100 : { *(.text.a) }
  .callee 0x6200 : { *(.text.b) }
  .data 0xa000 : { *(.payload .bss) }
}

;--- gc.ld
ENTRY(_start)
SECTIONS {
  /* The unreferenced writable input is removed before this is resolved. */
  .accepted 0x6000 : ONLY_IF_RO { *(.text.a .text.b .payload) }
  .caller 0x6100 : { *(.text.a) }
  .callee 0x6200 : { *(.text.b) }
  .data 0xa000 : { *(.payload .bss) }
}
