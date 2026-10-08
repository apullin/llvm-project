; RUN: llc -mtriple=i8085 -print-after=i8085-select-to-branch -o /dev/null %s 2>&1 | FileCheck %s
define i32 @masked(i1 %cond, i32 %x, i32 %y) nounwind {
  %s = select i1 %cond, i32 0, i32 %y
  %sum = add i32 %x, %s
  %safe = freeze i32 %sum
  ret i32 %safe
}
; CHECK: %sel.cond.frozen = freeze i1 %cond
; CHECK-NEXT: br i1 %sel.cond.frozen

define i32 @undefined(i32 %x, i32 %y) nounwind {
  %s = select i1 undef, i32 %y, i32 0
  %sum = add i32 %s, %x
  %safe = freeze i32 %sum
  ret i32 %safe
}
; CHECK: freeze i1 undef
; CHECK-NEXT: br i1

define i32 @poisoned(i32 %x, i32 %y) nounwind {
  %s = select i1 poison, i32 0, i32 %y
  %sum = add i32 %s, %x
  %safe = freeze i32 %sum
  ret i32 %safe
}
; CHECK: freeze i1 poison
; CHECK-NEXT: br i1
