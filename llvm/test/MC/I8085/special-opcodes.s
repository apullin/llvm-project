# RUN: llvm-mc -triple=i8085 -show-encoding < %s | FileCheck %s

        RLC
        RRC
        RAL
        RAR
        DAA
        STC
        CMC
        CMA
        DI
        EI
        RIM
        SIM
        XCHG
        XTHL
        PCHL
        HLT
        IN 0x10
        OUT 0x20

# CHECK: encoding: [0x07]
# CHECK: encoding: [0x0f]
# CHECK: encoding: [0x17]
# CHECK: encoding: [0x1f]
# CHECK: encoding: [0x27]
# CHECK: encoding: [0x37]
# CHECK: encoding: [0x3f]
# CHECK: encoding: [0x2f]
# CHECK: encoding: [0xf3]
# CHECK: encoding: [0xfb]
# CHECK: encoding: [0x20]
# CHECK: encoding: [0x30]
# CHECK: encoding: [0xeb]
# CHECK: encoding: [0xe3]
# CHECK: encoding: [0xe9]
# CHECK: encoding: [0x76]
# CHECK: encoding: [0xdb,0x10]
# CHECK: encoding: [0xd3,0x20]
