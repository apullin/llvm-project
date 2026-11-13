; RUN: llvm-mc -triple=tms9900 --show-encoding %s | FileCheck %s
; Regression test: validateTargetOperandClass previously matched every register
; against MCK_R11 (RET_REAL), silently encoding B *R0..R15 as 0x045B (B *R11).

    .text
; CHECK: B *R0 ; encoding: [0x04,0x50]
    B *R0
; CHECK: B *R5 ; encoding: [0x04,0x55]
    B *R5
; CHECK: B *R9 ; encoding: [0x04,0x59]
    B *R9
; CHECK: B *R11 ; encoding: [0x04,0x5b]
    B *R11
; CHECK: B *R15 ; encoding: [0x04,0x5f]
    B *R15
; CHECK: B *R11 ; encoding: [0x04,0x5b]
    RT
