// RUN: %i8085_clang -target i8085 -O2 -mllvm -verify-machineinstrs -c %s -o /dev/null
// RUN: %i8085_clang -target i8085 -Os -mllvm -verify-machineinstrs -c %s -o /dev/null
typedef unsigned char Bytes __attribute__((vector_size(4)));
Bytes divide(Bytes a) { return a / 2; }
