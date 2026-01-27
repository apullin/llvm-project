# RUN: not llvm-mc -triple=i8085 -filetype=asm %s 2>&1 | FileCheck %s

  dad a
  inr sp

# CHECK: error: invalid operand for instruction
# CHECK: dad a
# CHECK: error: invalid operand for instruction
# CHECK: inr sp
