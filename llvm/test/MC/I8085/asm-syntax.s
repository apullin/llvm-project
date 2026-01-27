# RUN: llvm-mc -triple=i8085 -show-encoding %s | FileCheck %s

  lxi b, 0x1234
  lxi bc, 0x1234
  inx d
  inx de
  dad h
  dad hl
  push psw
  pop psw
  mov m, a
  mov a, m

# CHECK: LXI B, 4660              ; encoding: [0x01,0x34,0x12]
# CHECK: LXI B, 4660              ; encoding: [0x01,0x34,0x12]
# CHECK: INX D                    ; encoding: [0x13]
# CHECK: INX D                    ; encoding: [0x13]
# CHECK: DAD H                    ; encoding: [0x29]
# CHECK: DAD H                    ; encoding: [0x29]
# CHECK: PUSH PSW                 ; encoding: [0xf5]
# CHECK: POP PSW                  ; encoding: [0xf1]
# CHECK: MOV M, A                 ; encoding: [0x77]
# CHECK: MOV A, M                 ; encoding: [0x7e]
