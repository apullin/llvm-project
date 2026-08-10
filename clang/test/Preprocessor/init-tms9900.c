// RUN: %clang_cc1 -E -dM -ffreestanding -triple=tms9900 < /dev/null | FileCheck %s
// REQUIRES: tms9900-registered-target

// char16_t uses a native 16-bit type, while char32_t must not inherit the
// target's 16-bit unsigned int default.
// CHECK: #define __CHAR16_TYPE__ unsigned short
// CHECK: #define __CHAR32_TYPE__ long unsigned int

// A volatile sig_atomic_t must fit in one indivisible TMS9900 word access.
// CHECK: #define __SIG_ATOMIC_MAX__ 32767
// CHECK: #define __SIG_ATOMIC_WIDTH__ 16
