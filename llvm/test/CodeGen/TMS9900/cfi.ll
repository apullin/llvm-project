; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=tms9900 -O2 -filetype=obj < %s -o %t
; RUN: llvm-objdump --dwarf=frames %t | FileCheck %s --check-prefix=FRAME

declare void @callee()

; CHECK-LABEL: nonleaf:
; CHECK:      DECT R10
; CHECK:      MOV R11,*R10
; CHECK:      .cfi_def_cfa_offset 2
; CHECK:      .cfi_offset 11, -2
; CHECK:      DECT R10
; CHECK:      .cfi_def_cfa_offset 4
; CHECK:      BL @callee
; CHECK:      INCT R10
; CHECK:      .cfi_def_cfa_offset 2
; CHECK:      MOV *R10+,R11
; CHECK:      .cfi_restore 11
; CHECK:      .cfi_def_cfa_offset 0
; CHECK:      B *R11
define void @nonleaf() uwtable {
  call void @callee()
  ret void
}

; CHECK-LABEL: with_frame:
; CHECK:      .cfi_offset 13, -4
; CHECK:      MOV R10,R13
; CHECK:      .cfi_def_cfa 13, 8
; CHECK:      BL @callee
; CHECK:      MOV R13,R10
; CHECK:      .cfi_def_cfa 10, 8
; CHECK:      .cfi_restore 13
; CHECK:      AI R10,6
; CHECK:      .cfi_def_cfa_offset 2
; CHECK:      MOV *R10+,R11
; CHECK:      .cfi_restore 11
; CHECK:      .cfi_def_cfa_offset 0
; CHECK:      B *R11
define void @with_frame() uwtable #0 {
  call void @callee()
  ret void
}

; The encoded FDE rows must carry the same return-address transitions. The
; exact instruction addresses are deliberately left unconstrained.
; FRAME: .eh_frame contents:
; FRAME: DW_CFA_offset: reg11 -2
; FRAME: DW_CFA_restore: reg11
; FRAME: DW_CFA_offset: reg13 -4
; FRAME: DW_CFA_restore: reg13
; FRAME: DW_CFA_restore: reg11

attributes #0 = { "frame-pointer"="all" }
