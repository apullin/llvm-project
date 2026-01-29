typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 *out = (volatile u8 *)0x0100;

static u8 data_bytes[4] = {1, 2, 3, 4};
static u16 data_word = 0xBEEF;
static u8 bss_bytes[4];
static u16 bss_word;

int main(void) {
  u8 ok = 1;
  if (data_bytes[0] != 1 || data_bytes[1] != 2 || data_bytes[2] != 3 ||
      data_bytes[3] != 4) {
    ok = 0;
  }
  if (data_word != 0xBEEF) {
    ok = 0;
  }
  if (bss_bytes[0] != 0 || bss_bytes[1] != 0 || bss_bytes[2] != 0 ||
      bss_bytes[3] != 0 || bss_word != 0) {
    ok = 0;
  }

  out[0] = ok ? 0xAA : 0xEE;
  out[1] = data_bytes[3];
  out[2] = (u8)(data_word & 0xFF);
  out[3] = (u8)(data_word >> 8);

  for (;;)
    ;
  return 0;
}
