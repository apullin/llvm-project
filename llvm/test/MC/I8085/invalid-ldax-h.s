# RUN: not llvm-mc -triple=i8085 %s 2>&1 | FileCheck %s

  LDAX H

# CHECK: error: invalid operand for instruction
