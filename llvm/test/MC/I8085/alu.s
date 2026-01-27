# RUN: llvm-mc -triple=i8085 -show-encoding < %s | FileCheck %s

  ADD B
  ADC C
  SUB D
  SBB E
  ANA H
  XRA L
  ORA M
  CMP A

  ADD M
  ADC M
  SUB M
  SBB M
  ANA M
  XRA M
  ORA M
  CMP M

  INR B
  DCR C

  ADI 0x01
  ACI 0x02
  SUI 0x03
  SBI 0x04
  ANI 0x05
  XRI 0x06
  ORI 0x07
  CPI 0x08

# CHECK: ADD B                    ; encoding: [0x80]
# CHECK: ADC C                    ; encoding: [0x89]
# CHECK: SUB D                    ; encoding: [0x92]
# CHECK: SBB E                    ; encoding: [0x9b]
# CHECK: ANA H                    ; encoding: [0xa4]
# CHECK: XRA L                    ; encoding: [0xad]
# CHECK: ORA M                    ; encoding: [0xb6]
# CHECK: CMP A                    ; encoding: [0xbf]

# CHECK: ADD M                    ; encoding: [0x86]
# CHECK: ADC M                    ; encoding: [0x8e]
# CHECK: SUB M                    ; encoding: [0x96]
# CHECK: SBB M                    ; encoding: [0x9e]
# CHECK: ANA M                    ; encoding: [0xa6]
# CHECK: XRA M                    ; encoding: [0xae]
# CHECK: ORA M                    ; encoding: [0xb6]
# CHECK: CMP M                    ; encoding: [0xbe]

# CHECK: INR B                    ; encoding: [0x04]
# CHECK: DCR C                    ; encoding: [0x0d]

# CHECK: ADI 1                    ; encoding: [0xc6,0x01]
# CHECK: ACI 2                    ; encoding: [0xce,0x02]
# CHECK: SUI 3                    ; encoding: [0xd6,0x03]
# CHECK: SBI 4                    ; encoding: [0xde,0x04]
# CHECK: ANI 5                    ; encoding: [0xe6,0x05]
# CHECK: XRI 6                    ; encoding: [0xee,0x06]
# CHECK: ORI 7                    ; encoding: [0xf6,0x07]
# CHECK: CPI 8                    ; encoding: [0xfe,0x08]
