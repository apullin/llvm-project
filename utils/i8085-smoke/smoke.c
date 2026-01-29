volatile unsigned char *out = (volatile unsigned char *)0x0100;

int main(void) {
  out[0] = 0x2A;
  out[1] = 0x55;
  for (;;) {
    out[2] = out[0] ^ out[1];
  }
  return 0;
}
