typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 *out = (volatile u8 *)0x0100;

__attribute__((noinline)) static u16 recurse(u8 n) {
  volatile u8 scratch[8];
  scratch[0] = n;
  if (n == 0) {
    return 1;
  }
  return (u16)scratch[0] + recurse((u8)(n - 1));
}

int main(void) {
  u16 result = recurse(12);
  out[0] = (u8)(result & 0xFF);
  out[1] = (u8)(result >> 8);
  for (;;)
    ;
  return 0;
}
