; RUN: llc -mattr=i8085,sram < %s -march=i8085 -verify-machineinstrs  | FileCheck %s

define signext i16 @fibonacci(i16 noundef signext %0) #0 {

; CHECK-LABEL: fibonacci:
; CHECK: LXI H, 65522
; CHECK: DAD	SP
; CHECK: SPHL
; CHECK: ADI 128
; CHECK: SBB A
; CHECK: ANI 128
; CHECK: JZ LBB{{.*}}
; CHECK: JMP LBB{{.*}}
; CHECK: JC LBB{{.*}}
; CHECK: JNZ LBB{{.*}}
; CHECK: SPHL
; CHECK: RET
  
 
 
  %2 = alloca i16, align 2
  %3 = alloca i16, align 2
  store i16 %0, ptr %3, align 2
  %4 = load i16, ptr %3, align 2
  %5 = sext i16 %4 to i32
  %6 = icmp sle i32 %5, 1
  br i1 %6, label %7, label %9

7:                                                ; preds = %1
  %8 = load i16, ptr %3, align 2
  store i16 %8, ptr %2, align 2
  br label %24

9:                                                ; preds = %1
  %10 = load i16, ptr %3, align 2
  %11 = sext i16 %10 to i32
  %12 = sub nsw i32 %11, 1
  %13 = trunc i32 %12 to i16
  %14 = call signext i16 @fibonacci(i16 noundef signext %13)
  %15 = sext i16 %14 to i32
  %16 = load i16, ptr %3, align 2
  %17 = sext i16 %16 to i32
  %18 = sub nsw i32 %17, 2
  %19 = trunc i32 %18 to i16
  %20 = call signext i16 @fibonacci(i16 noundef signext %19)
  %21 = sext i16 %20 to i32
  %22 = add nsw i32 %15, %21
  %23 = trunc i32 %22 to i16
  store i16 %23, ptr %2, align 2
  br label %24

24:                                               ; preds = %9, %7
  %25 = load i16, ptr %2, align 2
  ret i16 %25
}
