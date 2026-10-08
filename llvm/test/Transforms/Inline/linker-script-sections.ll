; RUN: opt -passes=always-inline -S %s | FileCheck %s --check-prefixes=OUTPUT,INPUT,NORMAL
; RUN: opt -passes='cgscc(inline)' -S %s | FileCheck %s --check-prefixes=OUTPUT,INPUT,NORMAL
; RUN: opt -passes=always-inline -inline-only-within-section=false -S %s | FileCheck %s --check-prefixes=OUTPUT,RELAXED,NORMAL
;
; Output placement is a correctness constraint even for alwaysinline and
; cannot be disabled through the preliminary input-section diagnostic flag.
@sink = external global i16

define i16 @helper() alwaysinline "linker_output_section"="script:0;" {
  %x = load volatile i16, ptr @sink
  ret i16 %x
}

; OUTPUT-LABEL: define i16 @different_output(
; OUTPUT: call i16 @helper()
define i16 @different_output() "linker_output_section"="script:1;" {
  %x = call i16 @helper()
  ret i16 %x
}

; OUTPUT-LABEL: define i16 @shared_output(
; OUTPUT-NOT: call
; OUTPUT: load volatile i16, ptr @sink
; OUTPUT: ret i16
define i16 @shared_output() "linker_output_section"="script:0;" {
  %x = call i16 @helper()
  ret i16 %x
}

define i16 @input_helper() alwaysinline "linker_input_section"=".helper" {
  %x = load volatile i16, ptr @sink
  ret i16 %x
}

; INPUT-LABEL: define i16 @different_input(
; INPUT: call i16 @input_helper()
; RELAXED-LABEL: define i16 @different_input(
; RELAXED-NOT: call
; RELAXED: load volatile i16, ptr @sink
; RELAXED: ret i16
define i16 @different_input() "linker_input_section"=".caller" {
  %x = call i16 @input_helper()
  ret i16 %x
}

define i16 @ordinary_helper() alwaysinline {
  %x = load volatile i16, ptr @sink
  ret i16 %x
}

; NORMAL-LABEL: define i16 @ordinary_caller(
; NORMAL-NOT: call
; NORMAL: load volatile i16, ptr @sink
; NORMAL: ret i16
define i16 @ordinary_caller() {
  %x = call i16 @ordinary_helper()
  ret i16 %x
}
