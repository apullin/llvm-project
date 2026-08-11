; RUN: llc -march=tms9900 -O2 < %s | FileCheck %s
; RUN: llc -march=tms9900 -O2 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -march=tms9900 -O2 -stop-after=postrapseudos < %s | FileCheck %s --check-prefix=POSTRA

declare i16 @foo(i16)
declare i16 @bar(i16)
declare i16 @inspect_frame(ptr)
declare ptr @llvm.frameaddress(i32 immarg)

;; Simple tail call: should use B instead of BL, no prologue/epilogue
; CHECK-LABEL: tail_call_simple:
; CHECK-NOT: DECT
; CHECK-NOT: BL
; CHECK: B{{[ 	]+}}@foo
define i16 @tail_call_simple(i16 %x) {
  %r = tail call i16 @foo(i16 %x)
  ret i16 %r
}

; The expanded tail branch must retain SP and argument-register liveness.
; POSTRA-LABEL: name: tail_call_simple
; POSTRA: TAIL_B @foo, implicit $r10, implicit $r0

;; An eligible musttail call has the same lowering as an ordinary tail call.
; CHECK-LABEL: musttail_call_simple:
; CHECK-NOT: DECT
; CHECK-NOT: BL
; CHECK: B{{[ \t]+}}@foo
define i16 @musttail_call_simple(i16 %x) {
  %r = musttail call i16 @foo(i16 %x)
  ret i16 %r
}

;; Non-tail call: result is modified, must use BL
; CHECK-LABEL: not_tail_call:
; CHECK: DECT
; CHECK: BL{{[ 	]+}}@foo
define i16 @not_tail_call(i16 %x) {
  %r = call i16 @foo(i16 %x)
  %s = add i16 %r, 1
  ret i16 %s
}

; Return expansion must retain the returned register as an implicit use.
; POSTRA-LABEL: name: not_tail_call
; POSTRA: RET_REAL implicit $r11, implicit $r0

;; Tail call with argument modification: still a tail call
; CHECK-LABEL: tail_call_with_arg:
; CHECK-NOT: DECT
; CHECK: INC
; CHECK: B{{[ 	]+}}@foo
define i16 @tail_call_with_arg(i16 %x) {
  %a = add i16 %x, 1
  %r = tail call i16 @foo(i16 %a)
  ret i16 %r
}

;; Conditional tail calls in both branches
; CHECK-LABEL: tail_call_diamond:
; CHECK: B{{[ 	]+}}@foo
; CHECK: B{{[ 	]+}}@bar
define i16 @tail_call_diamond(i16 %x) {
  %cmp = icmp sgt i16 %x, 10
  br i1 %cmp, label %then, label %else

then:
  %r1 = tail call i16 @foo(i16 %x)
  ret i16 %r1

else:
  %r2 = tail call i16 @bar(i16 %x)
  ret i16 %r2
}

;; A frame address remains valid until this function returns. Do not tear down
;; the frame and branch directly to a callee that can inspect that address.
; CHECK-LABEL: tail_call_with_frame_address:
; CHECK: MOV{{[ \t]+}}R10,R13
; CHECK: BL{{[ \t]+}}@inspect_frame
; CHECK-NOT: B{{[ \t]+}}@inspect_frame
; CHECK: MOV{{[ \t]+}}R13,R10
; CHECK: B{{[ \t]+}}*R11
define i16 @tail_call_with_frame_address() {
  %frame = call ptr @llvm.frameaddress(i32 0)
  %r = tail call i16 @inspect_frame(ptr %frame)
  ret i16 %r
}

;; Indirect tails need a real call/return descriptor, while retaining all
;; argument-register uses after post-RA pseudo expansion.
; CHECK-LABEL: tail_call_indirect:
; CHECK-NOT: BL
; CHECK: B{{[ \t]+}}*R{{[0-9]+}}
; POSTRA-LABEL: name: tail_call_indirect
; POSTRA: TAIL_B_IND {{.*}}, implicit $r10, implicit $r0, implicit $r1, implicit $r2
define i16 @tail_call_indirect(ptr %callee, i16 %a, i16 %b, i16 %c) {
  %r = tail call i16 %callee(i16 %a, i16 %b, i16 %c)
  ret i16 %r
}
