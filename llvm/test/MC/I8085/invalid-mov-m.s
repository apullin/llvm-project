# RUN: not llvm-mc -triple=i8085 %s 2>&1 | FileCheck %s

  mov m, m

# CHECK: error: invalid instruction: MOV M, M
