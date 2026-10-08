; RUN: llvm-mc -triple=tms9900 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s
;
; The N fields in Data Manual 3.5.9-3.5.13 are don't-care bits, not reserved
; zeros. Noncanonical encodings must decode without consuming spurious address
; extensions. Assemblers continue to emit canonical zero-N encodings.

.text
.short 0x0211, 0x1234
.short 0x0232, 0x0100
.short 0x0253, 0x00ff
.short 0x0274, 0x0f0f
.short 0x0295, 0x2222
.short 0x02b6
.short 0x02d7
.short 0x02ff, 0x1000
.short 0x031f, 0x0004
.short 0x035f
.short 0x037f
.short 0x039f
.short 0x03bf
.short 0x03df
.short 0x03ff
; >0320-033F remains undefined: these are opcode bits, not N bits.
.short 0x0320

; CHECK: 0: 02 11 12 34{{[ \t]+}}LI{{[ \t]+}}R1,4660
; CHECK-NEXT: 4: 02 32 01 00{{[ \t]+}}AI{{[ \t]+}}R2,256
; CHECK-NEXT: 8: 02 53 00 ff{{[ \t]+}}ANDI{{[ \t]+}}R3,255
; CHECK-NEXT: c: 02 74 0f 0f{{[ \t]+}}ORI{{[ \t]+}}R4,3855
; CHECK-NEXT: 10: 02 95 22 22{{[ \t]+}}CI{{[ \t]+}}R5,8738
; CHECK-NEXT: 14: 02 b6{{[ \t]+}}STWP{{[ \t]+}}R6
; CHECK-NEXT: 16: 02 d7{{[ \t]+}}STST{{[ \t]+}}R7
; CHECK-NEXT: 18: 02 ff 10 00{{[ \t]+}}LWPI{{[ \t]+}}4096
; CHECK-NEXT: 1c: 03 1f 00 04{{[ \t]+}}LIMI{{[ \t]+}}4
; CHECK-NEXT: 20: 03 5f{{[ \t]+}}IDLE
; CHECK-NEXT: 22: 03 7f{{[ \t]+}}RSET
; CHECK-NEXT: 24: 03 9f{{[ \t]+}}RTWP
; CHECK-NEXT: 26: 03 bf{{[ \t]+}}CKON
; CHECK-NEXT: 28: 03 df{{[ \t]+}}CKOF
; CHECK-NEXT: 2a: 03 ff{{[ \t]+}}LREX
; CHECK-NEXT: 2c: 03 20{{[ \t]+}}<unknown>
