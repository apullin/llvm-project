# RUN: llvm-mc -triple=i8085 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=DIS

  .text
  .globl foo
foo:
  mvi a, lo8(foo)
  mvi b, hi8(foo)
  lxi h, pm(foo)
  mvi c, pm_lo8(foo)
  mvi d, pm_hi8(foo)

# RELOC: R_I8085_LO8{{.*}}foo
# RELOC: R_I8085_HI8{{.*}}foo
# RELOC: R_I8085_PM{{.*}}foo
# RELOC: R_I8085_PM_LO8{{.*}}foo
# RELOC: R_I8085_PM_HI8{{.*}}foo

# DIS: MVI A,
# DIS: MVI B,
# DIS: LXI H,
# DIS: MVI C,
# DIS: MVI D,
