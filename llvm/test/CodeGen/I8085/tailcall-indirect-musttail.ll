; RUN: not --crash llc -mtriple=i8085 < %s -o /dev/null 2>&1 | FileCheck %s
define void @mandatory(ptr %p) {
  musttail call void %p(ptr %p)
  ret void
}
; CHECK: I8085 cannot lower this musttail call
