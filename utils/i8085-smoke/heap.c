typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 *out_lo = (volatile u8 *)0x0100;
volatile u8 *out_hi = (volatile u8 *)0x0101;
volatile u8 *out_off = (volatile u8 *)0x0102;

static u8 heap[64];
static u8 heap_off;
static u8 *volatile heap_a;
static u8 *volatile heap_b;

static void *heap_alloc(u8 size) {
  if ((u16)heap_off + size > (u16)sizeof(heap)) {
    return 0;
  }
  u8 *ptr = &heap[heap_off];
  heap_off = (u8)(heap_off + size);
  return ptr;
}

int main(void) {
  u16 sum = 0;
  u8 i;

  heap_a = (u8 *)heap_alloc(16);
  heap_b = (u8 *)heap_alloc(12);

  if (!heap_a || !heap_b) {
    *out_lo = 0xEE;
    for (;;)
      ;
  }

  for (i = 0; i < 16; ++i) {
    heap_a[i] = (u8)(i + 1);
  }
  for (i = 0; i < 12; ++i) {
    heap_b[i] = (u8)(0xA0 + i);
  }
  for (i = 0; i < 16; ++i) {
    sum = (u16)(sum + heap_a[i]);
  }
  for (i = 0; i < 12; ++i) {
    sum = (u16)(sum + heap_b[i]);
  }

  *out_lo = (u8)(sum & 0xFF);
  *out_hi = (u8)(sum >> 8);
  *out_off = heap_off;
  for (;;)
    ;
  return 0;
}
