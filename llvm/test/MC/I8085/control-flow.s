# RUN: llvm-mc -triple=i8085 -show-encoding < %s | FileCheck %s

foo:
  JMP foo
  JZ foo
  JNZ foo
  JC foo
  JNC foo
  JP foo
  JM foo
  JPO foo
  JPE foo
  CALL foo
  CNZ foo
  CZ foo
  CNC foo
  CC foo
  CPO foo
  CPE foo
  CP foo
  CM foo
  RET
  RNZ
  RZ
  RNC
  RC
  RPO
  RPE
  RP
  RM
  RST 0
  RST 7

# CHECK-LABEL: foo:
# CHECK: JMP foo{{.*}}encoding: [0xc3
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JZ foo{{.*}}encoding: [0xca
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JNZ foo{{.*}}encoding: [0xc2
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JC foo{{.*}}encoding: [0xda
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JNC foo{{.*}}encoding: [0xd2
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JP foo{{.*}}encoding: [0xf2
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JM foo{{.*}}encoding: [0xfa
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JPO foo{{.*}}encoding: [0xe2
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: JPE foo{{.*}}encoding: [0xea
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CALL foo{{.*}}encoding: [0xcd
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CNZ foo{{.*}}encoding: [0xc4
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CZ foo{{.*}}encoding: [0xcc
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CNC foo{{.*}}encoding: [0xd4
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CC foo{{.*}}encoding: [0xdc
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CPO foo{{.*}}encoding: [0xe4
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CPE foo{{.*}}encoding: [0xec
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CP foo{{.*}}encoding: [0xf4
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: CM foo{{.*}}encoding: [0xfc
# CHECK-NEXT: fixup A - offset: 0, value: foo, kind: fixup_16
# CHECK: RET{{.*}}encoding: [0xc9]
# CHECK: RNZ{{.*}}encoding: [0xc0]
# CHECK: RZ{{.*}}encoding: [0xc8]
# CHECK: RNC{{.*}}encoding: [0xd0]
# CHECK: RC{{.*}}encoding: [0xd8]
# CHECK: RPO{{.*}}encoding: [0xe0]
# CHECK: RPE{{.*}}encoding: [0xe8]
# CHECK: RP{{.*}}encoding: [0xf0]
# CHECK: RM{{.*}}encoding: [0xf8]
# CHECK: RST 0{{.*}}encoding: [0xc7]
# CHECK: RST 7{{.*}}encoding: [0xff]
