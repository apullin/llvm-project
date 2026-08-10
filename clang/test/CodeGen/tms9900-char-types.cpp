// RUN: %clang_cc1 -triple tms9900 -std=c++11 -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// REQUIRES: tms9900-registered-target

static_assert(sizeof(char16_t) == 2, "char16_t must hold UTF-16 code units");
static_assert(sizeof(char32_t) == 4, "char32_t must hold UTF-32 code units");
static_assert(sizeof(u'a') == 2, "UTF-16 character literal width");
static_assert(sizeof(U'a') == 4, "UTF-32 character literal width");

// CHECK-LABEL: define dso_local {{.*}}i32 @_Z11pass_char32Di(i32 {{.*}}%value)
char32_t pass_char32(char32_t value) { return value; }
