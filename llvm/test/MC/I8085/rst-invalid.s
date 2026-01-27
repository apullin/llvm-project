# RUN: not llvm-mc -triple=i8085 %s 2>&1 | FileCheck %s

        RST 8

# CHECK: error: RST immediate out of range (expected 0..7)
