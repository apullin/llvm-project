# RUN: not llvm-mc -triple=i8085 %s 2>&1 | FileCheck %s

  reti

# CHECK: error: invalid instruction
# CHECK-NEXT: reti
