typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 *out = (volatile u8 *)0x0100;

static u8 heap[64];
static u8 heap_off;

static void *heap_alloc(u8 size) {
  if ((u16)heap_off + size > (u16)sizeof(heap)) {
    return 0;
  }
  u8 *ptr = &heap[heap_off];
  heap_off = (u8)(heap_off + size);
  return ptr;
}

int main(void) {
  u8 *a = (u8 *)heap_alloc(16);
  u8 *b = (u8 *)heap_alloc(12);
  u16 sum = 0;
  u8 i;

  if (!a || !b) {
    out[0] = 0xEE;
    for (;;)
      ;
  }

  for (i = 0; i < 16; ++i) {
    a[i] = (u8)(i + 1);
  }
  for (i = 0; i < 12; ++i) {
    b[i] = (u8)(0xA0 + i);
  }
  for (i = 0; i < 16; ++i) {
    sum = (u16)(sum + a[i]);
  }
  for (i = 0; i < 12; ++i) {
    sum = (u16)(sum + b[i]);
  }

  out[0] = (u8)(sum & 0xFF);
  out[1] = (u8)(sum >> 8);
  out[2] = heap_off;
  for (;;)
    ;
  return 0;
}
