; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs | FileCheck %s
;
; Regression test for negative block numbers in indirect call return labels.
;
; The CALL_INDIRECT pseudo splits the block and creates a return label.
; Previously, mid-expansion RenumberBlocks() could leave the return MBB
; with number -1 (temporarily evicted), and if CachedMCSymbol was created
; during that window, the label would be permanently baked as LBBn_-1.
;
; This test verifies:
;   1. Single indirect call return label is a valid (non-negative) block ref
;   2. Multiple indirect calls in one function all get valid labels
;   3. Indirect call interleaved with direct calls

; --- Single indirect call: label must not be negative ---
define i16 @single_indirect(ptr %fn, i16 %a) {
; CHECK-LABEL: single_indirect:
; CHECK:       LXI B, [[RET1:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[RET1]]:
; CHECK:       RET
entry:
  %r = call i16 %fn(i16 %a)
  ret i16 %r
}

; --- Multiple indirect calls: each gets a distinct valid label ---
define i16 @multi_indirect(ptr %fn1, ptr %fn2, i16 %a) {
; CHECK-LABEL: multi_indirect:
; CHECK:       LXI B, [[RET2:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[RET2]]:
; CHECK:       LXI B, [[RET3:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[RET3]]:
; CHECK:       RET
entry:
  %r1 = call i16 %fn1(i16 %a)
  %r2 = call i16 %fn2(i16 %r1)
  ret i16 %r2
}

; --- Three indirect calls (stress the renumbering) ---
define i16 @triple_indirect(ptr %fn1, ptr %fn2, ptr %fn3, i16 %a) {
; CHECK-LABEL: triple_indirect:
; CHECK:       LXI B, [[R4:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[R4]]:
; CHECK:       LXI B, [[R5:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[R5]]:
; CHECK:       LXI B, [[R6:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[R6]]:
; CHECK:       RET
entry:
  %r1 = call i16 %fn1(i16 %a)
  %r2 = call i16 %fn2(i16 %r1)
  %r3 = call i16 %fn3(i16 %r2)
  ret i16 %r3
}

; --- Indirect call mixed with direct call ---
declare i16 @direct_target(i16)

define i16 @mixed_calls(ptr %fn, i16 %a) {
; CHECK-LABEL: mixed_calls:
; CHECK:       CALL direct_target
; CHECK:       LXI B, [[R7:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[R7]]:
; CHECK:       CALL direct_target
; CHECK:       RET
entry:
  %r1 = call i16 @direct_target(i16 %a)
  %r2 = call i16 %fn(i16 %r1)
  %r3 = call i16 @direct_target(i16 %r2)
  ret i16 %r3
}

; --- Indirect call in a loop (callback pattern, like the wolfIP bug) ---
define void @callback_loop(ptr %cb, ptr %data, i16 %n) {
; CHECK-LABEL: callback_loop:
; CHECK:       LXI B, [[RL:LBB[0-9]+_[0-9]+]]
; CHECK-NEXT:  PUSH B
; CHECK-NEXT:  PCHL
; CHECK:       [[RL]]:
entry:
  br label %loop

loop:
  %i = phi i16 [0, %entry], [%i.next, %loop]
  call void %cb(ptr %data, i16 %i)
  %i.next = add i16 %i, 1
  %cmp = icmp ult i16 %i.next, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}
