; RUN: not llvm-mc -triple tms9900 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

  jeq far_target
  .space 65536
far_target:
  idle

; CHECK: error: fixup value out of range
