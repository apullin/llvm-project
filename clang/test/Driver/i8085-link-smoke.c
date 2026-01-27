// REQUIRES: i8085-registered-target, lld
// RUN: %clang --target=i8085-unknown-elf -fuse-ld=lld %s -o %t
// RUN: test -f %t

int main(void) { return 0; }
