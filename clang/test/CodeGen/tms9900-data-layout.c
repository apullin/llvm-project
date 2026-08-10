// RUN: %clang_cc1 -triple tms9900 -emit-llvm -o - %s | FileCheck %s
// REQUIRES: tms9900-registered-target

// CHECK: target datalayout = "E-p:16:16-i8:8:8-i16:16:16-i32:16:32-i64:16:16-f32:16:16-f64:16:16-n16-S32"

struct float_record {
  unsigned char first;
  float value;
  unsigned char last;
};

struct wide_record {
  unsigned char first;
  unsigned long long value;
  unsigned char last;
};

struct double_record {
  unsigned char first;
  double value;
  unsigned char last;
};

struct default_aligned_record {
  unsigned char value __attribute__((aligned));
};

// Natural LLVM structs must now represent Clang's two-byte scalar ABI without
// packed-struct workarounds.
// CHECK: %struct.float_record = type { i8, float, i8 }
// CHECK: %struct.wide_record = type { i8, i64, i8 }
// CHECK: %struct.double_record = type { i8, double, i8 }

struct float_record float_object;
struct wide_record wide_object;
struct double_record double_object;

_Static_assert(sizeof(struct float_record) == 8, "float record size");
_Static_assert(__builtin_offsetof(struct float_record, value) == 2,
               "float field offset");
_Static_assert(__builtin_offsetof(struct float_record, last) == 6,
               "float tail offset");

_Static_assert(sizeof(struct wide_record) == 12, "i64 record size");
_Static_assert(__builtin_offsetof(struct wide_record, value) == 2,
               "i64 field offset");
_Static_assert(__builtin_offsetof(struct wide_record, last) == 10,
               "i64 tail offset");

_Static_assert(sizeof(struct double_record) == 12, "double record size");
_Static_assert(__builtin_offsetof(struct double_record, value) == 2,
               "double field offset");
_Static_assert(__builtin_offsetof(struct double_record, last) == 10,
               "double tail offset");

// A bare aligned attribute must agree with the scalar C ABI, not Clang's
// generic 16-byte default for otherwise-unconfigured targets.
_Static_assert(_Alignof(struct default_aligned_record) == 2,
               "default aligned attribute");
