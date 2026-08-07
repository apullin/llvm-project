; RUN: llc -mtriple=i8085-unknown-elf -O3 -verify-machineinstrs < %s -o /dev/null

; Exercise a call frame containing custom-inserter CFG expansion and enough
; GR32 pressure to spill.  Split blocks must inherit the active call-frame
; depth, and target-created spill/reload scratch definitions must be dead.

declare fastcc i32 @divide(i32, i32, ptr)
declare i32 @llvm.abs.i32(i32, i1 immarg)

define i32 @signed_divide(i32 %a, i32 %b) minsize optsize {
entry:
  %is.zero = icmp eq i32 %b, 0
  br i1 %is.zero, label %return, label %divide

divide:
  %sign = xor i32 %b, %a
  %abs.a = call i32 @llvm.abs.i32(i32 %a, i1 false)
  %abs.b = call i32 @llvm.abs.i32(i32 %b, i1 false)
  %quotient = call fastcc i32 @divide(i32 %abs.a, i32 %abs.b, ptr null)
  %negative = sub i32 0, %quotient
  %is.negative = icmp slt i32 %sign, 0
  %result = select i1 %is.negative, i32 %negative, i32 %quotient
  br label %return

return:
  %value = phi i32 [ %result, %divide ], [ 0, %entry ]
  ret i32 %value
}
