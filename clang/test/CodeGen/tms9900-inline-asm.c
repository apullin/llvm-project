// RUN: %clang_cc1 -triple tms9900 -O2 -S -o - %s | FileCheck %s
// REQUIRES: tms9900-registered-target

typedef unsigned short u16;

volatile u16 global_word;

// CHECK-LABEL: register_copy:
// CHECK:       ;APP
// CHECK:       MOV R0,R0
// CHECK:       ;NO_APP
u16 register_copy(u16 value) {
  u16 result;
  __asm__ volatile("MOV %1,%0" : "=r"(result) : "r"(value));
  return result;
}

// CHECK-LABEL: immediate_value:
// CHECK:       ;APP
// CHECK:       LI R0,42
// CHECK:       ;NO_APP
u16 immediate_value(void) {
  u16 result;
  __asm__ volatile("LI %0,%c1" : "=r"(result) : "i"(42));
  return result;
}

// CHECK-LABEL: memory_store:
// CHECK:       ;APP
// CHECK:       MOV R1,*R0
// CHECK:       ;NO_APP
void memory_store(volatile u16 *pointer, u16 value) {
  __asm__ volatile("MOV %1,%0" : "=m"(*pointer) : "r"(value));
}

// CHECK-LABEL: global_store:
// CHECK:       LI R1,global_word
// CHECK:       ;APP
// CHECK:       MOV R0,*R1
// CHECK:       ;NO_APP
void global_store(u16 value) {
  __asm__ volatile("MOV %1,%0" : "=m"(global_word) : "r"(value));
}
