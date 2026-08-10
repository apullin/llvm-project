; RUN: llvm-mc -filetype=obj -triple tms9900 < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s

  .globl ext
  jmp ext

; An unresolved branch has a temporary -1 displacement in the object.  At
; address zero that decodes as a branch to address zero, not as NOP (which is
; JMP to the following instruction).
; CHECK: {{[0-9a-f]+}}: 10 ff{{[ \t]+}}JMP{{[ \t]+}}0
