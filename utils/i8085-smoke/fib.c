typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 *out_lo = (volatile u8 *)0x0100;
volatile u8 *out_hi = (volatile u8 *)0x0101;
volatile u8 *out_n = (volatile u8 *)0x0102;

static u16 fib(u8 n) {
  u16 a = 0;
  u16 b = 1;
  u8 i;

  for (i = 0; i < n; ++i) {
    u16 t = (u16)(a + b);
    a = b;
    b = t;
  }
  return a;
}

int main(void) {
  u8 n = 16;
  u16 r = fib(n);

  *out_lo = (u8)(r & 0xFF);
  *out_hi = (u8)(r >> 8);
  *out_n = n;

  for (;;)
    ;
  return 0;
}
