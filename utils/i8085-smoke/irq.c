typedef unsigned char u8;

volatile u8 irq_flag;
volatile u8 *out = (volatile u8 *)0x0100;

int main(void) {
  irq_flag = 0;
  __asm__ volatile("EI");
  while (!irq_flag)
    ;
  out[0] = irq_flag;
  for (;;)
    ;
  return 0;
}
