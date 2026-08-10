// RUN: %clang_cc1 -E -dM -ffreestanding -triple=tms9900 < /dev/null | FileCheck %s
// REQUIRES: tms9900-registered-target

// A volatile sig_atomic_t must fit in one indivisible TMS9900 word access.
// CHECK: #define __SIG_ATOMIC_MAX__ 32767
// CHECK: #define __SIG_ATOMIC_WIDTH__ 16
