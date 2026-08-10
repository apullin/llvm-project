// RUN: %clang_cc1 -triple tms9900 -emit-llvm -o - %s | FileCheck %s
// REQUIRES: tms9900-registered-target

typedef unsigned short u16;

struct pair {
  u16 first;
  u16 second;
};

// C aggregates are passed indirectly using an aligned byval pointer.
// CHECK-LABEL: define dso_local zeroext i16 @sum_pair(
// CHECK-SAME: ptr noundef byval(%struct.pair) align 2 %value)
u16 sum_pair(struct pair value) { return value.first + value.second; }

// C aggregate returns use a hidden result pointer before ordinary arguments.
// CHECK-LABEL: define dso_local void @make_pair(
// CHECK-SAME: ptr dead_on_unwind noalias writable sret(%struct.pair) align 2 %agg.result,
// CHECK-SAME: i16 noundef zeroext %first, i16 noundef zeroext %second)
struct pair make_pair(u16 first, u16 second) {
  struct pair result = {first, second};
  return result;
}

// int and pointers are both native 16-bit ABI values.
// CHECK-LABEL: define dso_local i16 @scalar_widths(i16 noundef %value, ptr noundef %pointer)
int scalar_widths(int value, void *pointer) {
  return value + (pointer != (void *)0);
}
