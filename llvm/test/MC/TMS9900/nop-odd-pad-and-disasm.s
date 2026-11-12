; RUN: llvm-mc -triple=tms9900 -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s

	.text
	DEF odd_pad_and_unknown
odd_pad_and_unknown:
	.byte 0x42
	.p2align 1
	; 0x0020 is an undefined opcode whose Ts field looks like symbolic (2 words);
	; failed decode must advance by 2 bytes, not 4, so B *R11 at +4 decodes.
	.short 0x0020
	B *R11

; CHECK: 42 00
; CHECK: 00 20
; CHECK: 04 5b B *R11
