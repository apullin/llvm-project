# RUN: not llvm-mc -triple=i8085 -filetype=asm %s 2>&1 | FileCheck %s

  mvi a, 256
  adi 300
  lxi h, 70000
  out -129

# CHECK: error: invalid operand for instruction
# CHECK: mvi a, 256
# CHECK: error: invalid operand for instruction
# CHECK: adi 300
# CHECK: error: invalid operand for instruction
# CHECK: lxi h, 70000
# CHECK: error: invalid operand for instruction
# CHECK: out -129
