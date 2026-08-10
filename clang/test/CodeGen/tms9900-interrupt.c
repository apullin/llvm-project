// RUN: %clang_cc1 -triple tms9900-unknown-none -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -triple tms9900-unknown-none -S -o - %s | FileCheck %s --check-prefix=ASM

__attribute__((interrupt)) void handler(void) {}

// IR: @llvm.compiler.used = appending global
// IR-SAME: @handler
// IR: define{{.*}} void @handler() #[[ATTRS:[0-9]+]]
// IR: attributes #[[ATTRS]] = {
// IR-SAME: noinline
// IR-SAME: "interrupt"

// ASM-LABEL: handler:
// ASM: RTWP
