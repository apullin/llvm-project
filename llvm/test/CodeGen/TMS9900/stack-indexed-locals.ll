; RUN: llc -mtriple=tms9900 -O2 -verify-machineinstrs < %s | FileCheck %s

declare void @escape(ptr)

define i16 @word_stack_local(i16 %x) {
; CHECK-LABEL: word_stack_local:
; CHECK:       MOV [[REG:R[0-9]+]],@2(R10)
; CHECK:       MOV *R10,[[REG2:R[0-9]+]]
  %store_slot = alloca i16, align 2
  %load_slot = alloca i16, align 2
  store volatile i16 %x, ptr %store_slot, align 2
  %value = load volatile i16, ptr %load_slot, align 2
  ret i16 %value
}

define i16 @byte_stack_local(i16 %x) {
; CHECK-LABEL: byte_stack_local:
; CHECK:       MOVB [[REG:R[0-9]+]],@{{[0-9]+}}(R10)
; CHECK:       MOVB @{{[0-9]+}}(R10),[[REG2:R[0-9]+]]
  %store_slot = alloca i8, align 1
  %load_slot = alloca i8, align 1
  %byte = trunc i16 %x to i8
  store volatile i8 %byte, ptr %store_slot, align 1
  %value = load volatile i8, ptr %load_slot, align 1
  %extended = zext i8 %value to i16
  ret i16 %extended
}

define void @stack_local_offsets(i16 %word, i16 %byte) {
; CHECK-LABEL: stack_local_offsets:
; CHECK:       MOV [[WORD:R[0-9]+]],@{{[0-9]+}}(R10)
; CHECK:       MOVB [[BYTE:R[0-9]+]],@{{[0-9]+}}(R10)
  %words = alloca [4 x i16], align 2
  %bytes = alloca [8 x i8], align 1
  %word_ptr = getelementptr [4 x i16], ptr %words, i16 0, i16 2
  %byte_ptr = getelementptr [8 x i8], ptr %bytes, i16 0, i16 5
  store volatile i16 %word, ptr %word_ptr, align 2
  %byte_value = trunc i16 %byte to i8
  store volatile i8 %byte_value, ptr %byte_ptr, align 1
  ret void
}

define i16 @shared_stack_address() {
; CHECK-LABEL: shared_stack_address:
; CHECK:       MOV R10,[[ADDR:R[0-9]+]]
; CHECK:       MOV *[[ADDR]],[[FIRST:R[0-9]+]]
; CHECK:       MOV *[[ADDR]],[[SECOND:R[0-9]+]]
  %slot = alloca i16, align 2
  %first = load volatile i16, ptr %slot, align 2
  %second = load volatile i16, ptr %slot, align 2
  %sum = add i16 %first, %second
  ret i16 %sum
}

define void @escaped_stack_address(i16 %x) {
; CHECK-LABEL: escaped_stack_address:
; CHECK:       MOV R10,[[ADDR:R[0-9]+]]
; CHECK:       BL @escape
  %slot = alloca i16, align 2
  store i16 %x, ptr %slot, align 2
  call void @escape(ptr %slot)
  ret void
}
