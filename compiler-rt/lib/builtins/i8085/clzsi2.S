// Minimal i8085 __clzsi2 in assembly to avoid codegen pitfalls.
// Returns the count of leading zeroes in a 32-bit value.

  .section .text.__clzsi2, "ax", @progbits
  .globl __clzsi2
  .type __clzsi2, @function
__clzsi2:
  // HL = SP
  LXI H, 0x0000
  DAD SP

  // Move to argument high byte (SP + 5)
  INX H
  INX H
  INX H
  INX H
  INX H
  MOV A, M
  ORA A
  JZ 1f
  CALL clz8
  MOV C, A
  MVI B, 0x00
  JMP clzret

1:
  // Byte 2 (SP + 4)
  DCX H
  MOV A, M
  ORA A
  JZ 2f
  CALL clz8
  ADI 0x08
  MOV C, A
  MVI B, 0x00
  JMP clzret

2:
  // Byte 1 (SP + 3)
  DCX H
  MOV A, M
  ORA A
  JZ 3f
  CALL clz8
  ADI 0x10
  MOV C, A
  MVI B, 0x00
  JMP clzret

3:
  // Byte 0 (SP + 2)
  DCX H
  MOV A, M
  ORA A
  JZ 4f
  CALL clz8
  ADI 0x18
  MOV C, A
  MVI B, 0x00
  JMP clzret

4:
  // All bytes zero -> return 32.
  MVI C, 0x20
  MVI B, 0x00
  JMP clzret

clzret:
  XRA A
  MOV D, A
  MOV E, A
  RET

// A = byte, returns A = leading zeros (0..8)
clz8:
  ORA A
  JNZ clz8_loop
  MVI A, 0x08
  RET

clz8_loop:
  MVI C, 0x00
clz8_iter:
  MOV B, A
  ANI 0x80
  JNZ clz8_done
  MOV A, B
  STC
  CMC
  RAL
  INR C
  JMP clz8_iter

clz8_done:
  MOV A, C
  RET
