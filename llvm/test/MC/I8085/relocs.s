# RUN: llvm-mc -triple=i8085 -filetype=obj < %s | llvm-objdump -r - | FileCheck %s

  jmp foo
  call bar
  lhld baz

# CHECK: RELOCATION RECORDS FOR [.text]:
# CHECK: R_I8085_16{{.*}}foo
# CHECK: R_I8085_16{{.*}}bar
# CHECK: R_I8085_16{{.*}}baz
