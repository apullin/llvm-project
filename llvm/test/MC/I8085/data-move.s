# RUN: llvm-mc -triple=i8085 -show-encoding < %s | FileCheck %s

  LXI B, 0x1234
  LXI D, 0x5678
  LXI H, 0x9abc
  LXI SP, 0x0fed

  INX B
  INX D
  INX H
  INX SP

  DCX B
  DCX D
  DCX H
  DCX SP

  DAD B
  DAD D
  DAD H
  DAD SP

  LDAX B
  LDAX D
  STAX B
  STAX D

  LDA 0x1234
  STA 0x5678
  LHLD 0x9abc
  SHLD 0x0fed

  MVI A, 0x42
  MVI B, 0x12
  MVI M, 0x34

  MOV A, B
  MOV E, D
  MOV M, C
  MOV D, M

  PUSH B
  POP B
  PUSH D
  POP D
  PUSH H
  POP H

  SPHL

# CHECK: LXI B, 4660              ; encoding: [0x01,0x34,0x12]
# CHECK: LXI D, 22136             ; encoding: [0x11,0x78,0x56]
# CHECK: LXI H, 39612             ; encoding: [0x21,0xbc,0x9a]
# CHECK: LXI SP, 4077             ; encoding: [0x31,0xed,0x0f]

# CHECK: INX B                    ; encoding: [0x03]
# CHECK: INX D                    ; encoding: [0x13]
# CHECK: INX H                    ; encoding: [0x23]
# CHECK: INX SP                   ; encoding: [0x33]

# CHECK: DCX B                    ; encoding: [0x0b]
# CHECK: DCX D                    ; encoding: [0x1b]
# CHECK: DCX H                    ; encoding: [0x2b]
# CHECK: DCX SP                   ; encoding: [0x3b]

# CHECK: DAD B                    ; encoding: [0x09]
# CHECK: DAD D                    ; encoding: [0x19]
# CHECK: DAD H                    ; encoding: [0x29]
# CHECK: DAD SP                   ; encoding: [0x39]

# CHECK: LDAX B                   ; encoding: [0x0a]
# CHECK: LDAX D                   ; encoding: [0x1a]
# CHECK: STAX B                   ; encoding: [0x02]
# CHECK: STAX D                   ; encoding: [0x12]

# CHECK: LDA 4660                 ; encoding: [0x3a,0x34,0x12]
# CHECK: STA 22136                ; encoding: [0x32,0x78,0x56]
# CHECK: LHLD 39612               ; encoding: [0x2a,0xbc,0x9a]
# CHECK: SHLD 4077                ; encoding: [0x22,0xed,0x0f]

# CHECK: MVI A, 66                ; encoding: [0x3e,0x42]
# CHECK: MVI B, 18                ; encoding: [0x06,0x12]
# CHECK: MVI M, 52                ; encoding: [0x36,0x34]

# CHECK: MOV A, B                 ; encoding: [0x78]
# CHECK: MOV E, D                 ; encoding: [0x5a]
# CHECK: MOV M, C                 ; encoding: [0x71]
# CHECK: MOV D, M                 ; encoding: [0x56]

# CHECK: PUSH B                   ; encoding: [0xc5]
# CHECK: POP B                    ; encoding: [0xc1]
# CHECK: PUSH D                   ; encoding: [0xd5]
# CHECK: POP D                    ; encoding: [0xd1]
# CHECK: PUSH H                   ; encoding: [0xe5]
# CHECK: POP H                    ; encoding: [0xe1]

# CHECK: SPHL                     ; encoding: [0xf9]
