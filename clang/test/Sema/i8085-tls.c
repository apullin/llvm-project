// REQUIRES: i8085-registered-target
// RUN: %clang_cc1 -triple i8085-unknown-elf -fsyntax-only -verify %s

__thread int tls_var; // expected-error {{thread-local storage is not supported for the current target}}
