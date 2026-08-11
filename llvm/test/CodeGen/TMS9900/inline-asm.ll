; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=tms9900 -O2 -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=MIR
;
; Test inline assembly support.
; The TMS9900 backend should pass through inline asm and handle
; register constraints.

@asm_symbol = external global i16

; --- Simple inline asm with no operands ---
; NOP is accepted as an input alias and canonicalized to JMP 0 in textual
; assembly.  Address-aware object disassembly prints the NOP alias.
; CHECK-LABEL: asm_nop:
; CHECK: ;APP
; CHECK: JMP{{[ \t]+}}0
; CHECK: ;NO_APP
; CHECK: B{{[ \t]+}}*R11

define void @asm_nop() {
  call void asm sideeffect "NOP", ""()
  ret void
}

; --- Inline asm with input register constraint ---
; CHECK-LABEL: asm_with_input:
; CHECK: ;APP
; CHECK: ;NO_APP
; CHECK: B{{[ \t]+}}*R11

define void @asm_with_input(i16 %val) {
  call void asm sideeffect "", "r"(i16 %val)
  ret void
}

; --- Register and immediate operands ---
; CHECK-LABEL: asm_register_immediate:
; CHECK: ;APP
; CHECK: LI R0,42
; CHECK: MOV R0,R0
; CHECK: ;NO_APP

define i16 @asm_register_immediate(i16 %val) {
  %result = call i16 asm sideeffect "LI $0,${1:c}\0AMOV $2,$0",
                                      "=r,i,r"(i16 42, i16 %val)
  ret i16 %result
}

; --- Memory output through a computed pointer ---
; A generic "m" operand must select successfully and print as *Rn.
; CHECK-LABEL: asm_memory_store:
; CHECK: ;APP
; CHECK: MOV R1,*R0
; CHECK: ;NO_APP

define void @asm_memory_store(ptr %pointer, i16 %value) {
  call void asm sideeffect "MOV $1,$0", "=*m,r"(
      ptr elementtype(i16) %pointer, i16 %value)
  ret void
}

; --- Memory input from a frame object ---
; CHECK-LABEL: asm_memory_local:
; CHECK: ;APP
; CHECK: MOV *R{{[0-9]+}},R0
; CHECK: ;NO_APP

define i16 @asm_memory_local(i16 %value) {
  %slot = alloca i16, align 2
  store i16 %value, ptr %slot, align 2
  %result = call i16 asm sideeffect "MOV $1,$0", "=r,*m"(
      ptr elementtype(i16) %slot)
  ret i16 %result
}

; --- Status-register clobber ---
; The C/C++ "cc" clobber is represented as ~{cc} in LLVM IR and must become
; an explicit ST definition so machine scheduling cannot preserve stale flags
; across the inline assembly.
; MIR-LABEL: name: asm_cc_clobber
; MIR: INLINEASM {{.*}} implicit-def early-clobber $st

define void @asm_cc_clobber() {
  call void asm sideeffect "", "~{cc}"()
  ret void
}

; --- Symbolic immediate operands ---
; A generic symbolic operand is an address value, not a symbolic-addressing
; memory operand. The inline-asm template supplies @ itself when it needs the
; latter form.
; CHECK-LABEL: asm_symbolic_immediate:
; CHECK: LI R0,asm_symbol
; CHECK-NOT: LI R0,@asm_symbol

define void @asm_symbolic_immediate() {
  call void asm sideeffect "LI R0,$0", "s"(ptr @asm_symbol)
  ret void
}

; CHECK-LABEL: asm_symbolic_offset:
; CHECK: LI R0,asm_symbol+2

define void @asm_symbolic_offset() {
  call void asm sideeffect "LI R0,$0", "s"(
      ptr getelementptr (i8, ptr @asm_symbol, i16 2))
  ret void
}
