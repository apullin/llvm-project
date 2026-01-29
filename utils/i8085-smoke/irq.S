  .text
  .globl isr_rst55
isr_rst55:
  PUSH PSW
  PUSH H
  LXI H, irq_flag
  MVI M, 0x5A
  POP H
  POP PSW
  RET
