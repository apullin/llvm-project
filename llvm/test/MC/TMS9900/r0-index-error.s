; RUN: not llvm-mc -triple=tms9900 %s -o /dev/null 2>&1 | FileCheck %s

	.text
; CHECK: error: R0 cannot be used as an index register
	MOV @2(R0),R1
; CHECK: error: R0 cannot be used as an index register
	DEC @-2(R0)
