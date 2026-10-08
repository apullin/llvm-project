// REQUIRES: i8085-sim
// RUN: %i8085_clang -target i8085 -c %i8085_sdk/sysroot/crt/crt0.S -o %t.crt.o
// RUN: %i8085_clang -target i8085 -O1 -mllvm -verify-machineinstrs -ffreestanding -fno-builtin -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 2000000 -d %testdump_addr:8 %t.bin 2>&1 | FileCheck %s
// RUN: %i8085_clang -target i8085 -Oz -mllvm -verify-machineinstrs -ffreestanding -fno-builtin -c %s -o %t.o
// RUN: %i8085_lld -m i8085elf -T %i8085_sdk/sysroot/ldscripts/i8085-sim-flat.ld -o %t.elf %t.crt.o %t.o %i8085_sdk/sysroot/lib/libgcc.a
// RUN: %i8085_objcopy -O binary %t.elf %t.bin
// RUN: %i8085_sim -S -q -n 2000000 -d %testdump_addr:8 %t.bin 2>&1 | FileCheck %s

volatile unsigned char out[8] __attribute__((section(".testdump")));
unsigned long masks[2][2];
struct Node { struct Node *next; int index; };

__attribute__((noinline)) void collect(struct Node *node) {
  unsigned long words[2] = {0, 0};
  struct Node *last = node;
  unsigned char visits = 0;
  while (last->next) {
    words[0] |= ~masks[last->index][0];
    words[1] |= ~masks[last->index][1];
    last = last->next;
    ++visits;
  }
  words[0] |= ~masks[last->index][0];
  words[1] |= ~masks[last->index][1];
  out[0] = words[0] == ~0UL;
  out[1] = words[1] == ~0UL;
  out[2] = visits;
}

volatile int pressure[32];
#define EACH(X) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) \
  X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) \
  X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30)
#define LOAD(N) int x##N = pressure[N];
#define STORE(N) pressure[N] = x##N;
__attribute__((noinline)) unsigned long long count(int n, unsigned long long x) {
  while (n--) {
    EACH(LOAD)
    EACH(STORE)
    x += 513;
  }
  return x;
}

int main(void) {
  struct Node last = {0, 0}, first = {&last, 1};
  masks[0][0] = masks[0][1] = ~0UL;
  collect(&first);
  out[3] = count(3, 0xffffffffULL) == 0x100000602ULL;
  out[4] = 0xad;
  out[5] = 0xde;
  return 0;
}
// CHECK: FF00:{{ *}}01 01 01 01 AD DE 00 00
// CHECK: "halt":"hlt"
