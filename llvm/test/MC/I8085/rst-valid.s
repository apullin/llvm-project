# RUN: llvm-mc -triple=i8085 -show-encoding < %s | FileCheck %s

        RST 0
        RST 1
        RST 2
        RST 7

# CHECK: encoding: [0xc7]
# CHECK: encoding: [0xcf]
# CHECK: encoding: [0xd7]
# CHECK: encoding: [0xff]
