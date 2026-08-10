; RUN: not --crash llc -march=tms9900 -O2 < %s 2>&1 | FileCheck %s

; TMS9900 tail-call lowering cannot currently forward stack arguments.  A
; musttail call that requires one must fail rather than become an ordinary
; call and violate the IR contract.
; CHECK: LLVM ERROR: failed to perform tail call elimination on a call site marked musttail

declare i16 @callee(i16, i16, i16, i16, i16)

define i16 @caller(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) {
  %result = musttail call i16 @callee(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e)
  ret i16 %result
}
