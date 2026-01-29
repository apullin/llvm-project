typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 *out0 = (volatile u8 *)0x0100;
volatile u8 *out1 = (volatile u8 *)0x0101;
volatile u8 *out2 = (volatile u8 *)0x0102;
volatile u8 *out3 = (volatile u8 *)0x0103;

int main(void) {
  u16 v = 0x1234;
  u16 shr = (u16)(v >> 8);   /* 0x0012 */
  u16 shl = (u16)(v << 4);   /* 0x2340 */

  *out0 = (u8)shr;
  *out1 = (u8)shl;
  *out2 = (u8)(shl >> 8);
  *out3 = 0xA5;

  for (;;)
    ;
  return 0;
}
