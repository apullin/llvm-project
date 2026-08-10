; RUN: llc -mtriple=tms9900 -O2 < %s | FileCheck %s
;
; Test that the backend generates auto-increment addressing (*Rx+) for
; sequential memory access patterns in loops. The peephole pass combines
; a load/store through a pointer with a subsequent pointer increment into
; a single auto-increment instruction.

; --- Word copy loop should use auto-increment ---
; CHECK-LABEL: word_copy_loop:
; CHECK: MOV{{[ \t]+}}*R{{[0-9]+}}+,R{{[0-9]+}}
; CHECK: MOV{{[ \t]+}}R{{[0-9]+}},*R{{[0-9]+}}+

define void @word_copy_loop(ptr %dst, ptr %src, i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [0, %entry], [%next, %loop]
  %sp = getelementptr i16, ptr %src, i16 %i
  %dp = getelementptr i16, ptr %dst, i16 %i
  %v = load i16, ptr %sp
  store i16 %v, ptr %dp
  %next = add i16 %i, 1
  %cmp = icmp ult i16 %next, %n
  br i1 %cmp, label %loop, label %done
done:
  ret void
}

; --- Sum array using auto-increment load ---
; CHECK-LABEL: sum_array:
; CHECK: *R{{[0-9]+}}+

define i16 @sum_array(ptr %arr, i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [0, %entry], [%next, %loop]
  %sum = phi i16 [0, %entry], [%newsum, %loop]
  %p = getelementptr i16, ptr %arr, i16 %i
  %v = load i16, ptr %p
  %newsum = add i16 %sum, %v
  %next = add i16 %i, 1
  %cmp = icmp ult i16 %next, %n
  br i1 %cmp, label %loop, label %done
done:
  ret i16 %newsum
}

; Byte post-increment must retain the MOVB high-byte to LLVM low-byte
; conversion before arithmetic.
; CHECK-LABEL: sum_bytes:
; CHECK: MOVB{{[ \t]+}}*R{{[0-9]+}},R[[BYTE:[0-9]+]]
; CHECK-NEXT: SRL{{[ \t]+}}R[[BYTE]],8

define i16 @sum_bytes(ptr %arr, i16 %n) {
entry:
  br label %loop
loop:
  %p = phi ptr [ %arr, %entry ], [ %nextp, %loop ]
  %i = phi i16 [ 0, %entry ], [ %next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %newsum, %loop ]
  %v = load i8, ptr %p
  %wide = zext i8 %v to i16
  %newsum = add i16 %sum, %wide
  %nextp = getelementptr i8, ptr %p, i16 1
  %next = add i16 %i, 1
  %cmp = icmp ult i16 %next, %n
  br i1 %cmp, label %loop, label %done
done:
  ret i16 %newsum
}

; A normal low-byte value must be moved into MOVB's high byte before the
; byte store.
; CHECK-LABEL: fill_bytes:
; CHECK: SLA{{[ \t]+}}R[[VALUE:[0-9]+]],8
; CHECK: MOVB{{[ \t]+}}R[[VALUE]],*R{{[0-9]+}}

define void @fill_bytes(ptr %dst, i8 %value, i16 %n) {
entry:
  br label %loop
loop:
  %p = phi ptr [ %dst, %entry ], [ %nextp, %loop ]
  %i = phi i16 [ 0, %entry ], [ %next, %loop ]
  store i8 %value, ptr %p
  %nextp = getelementptr i8, ptr %p, i16 1
  %next = add i16 %i, 1
  %cmp = icmp ult i16 %next, %n
  br i1 %cmp, label %loop, label %done
done:
  ret void
}
