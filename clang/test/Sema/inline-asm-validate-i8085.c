// REQUIRES: i8085-registered-target
// RUN: %clang_cc1 -triple i8085-unknown-elf -fsyntax-only -verify %s

int ok_reg(int x) {
  asm volatile("" : "+r"(x));
  return x;
}

int ok_imm(void) {
  asm volatile("adi %0" : : "I"(7));
  return 0;
}

int bad_named(int x) {
  asm volatile("" : : "a"(x)); // expected-error {{invalid input constraint 'a' in asm}}
  return x;
}

int bad_out_named(int x) {
  asm volatile("" : "=a"(x)); // expected-error {{invalid output constraint '=a' in asm}}
  return x;
}

int bad_unknown(int x) {
  asm volatile("" : : "Z"(x)); // expected-error {{invalid input constraint 'Z' in asm}}
  return x;
}
