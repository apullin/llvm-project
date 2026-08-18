; Test that functions with "section" attribute are accepted, and jumptables are
; emitted in ".text".

; RUN: opt -S -passes=lowertypetests %s | FileCheck %s

target triple = "x86_64-unknown-linux-gnu"

; Type metadata that has no type-test users does not require global layout and
; may remain in an explicit section.
; CHECK: @unused = constant i32 0, section "yyy", !type ![[UNUSED:[0-9]+]]
@unused = constant i32 0, section "yyy", !type !1

; CHECK: @f = alias void (), ptr @[[JT:.*]]
; CHECK: define hidden void @f.cfi() section "xxx"

define void @f() section "xxx" !type !0 {
entry:
  ret void
}

define i1 @g() {
entry:
  %0 = call i1 @llvm.type.test(ptr @f, metadata !"_ZTSFvE")
  ret i1 %0
}

; CHECK: define private void @[[JT]]() #{{.*}} align {{.*}} {

declare i1 @llvm.type.test(ptr, metadata) nounwind readnone

!0 = !{i64 0, !"_ZTSFvE"}
!1 = !{i64 0, !"unused"}
