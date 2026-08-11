; RUN: llc -mtriple=tms9900 -O0 -verify-machineinstrs < %s -o /dev/null
;
; Scalarizing the vector comparison emits one soft-float call per lane and
; SELECT16 pseudos to materialize the predicate results. SelectionDAG may place
; a SELECT16 between ADJCALLSTACKDOWN and the following call. Blocks created by
; its custom inserter must retain the active outgoing call-frame size.

define void @vector_fcmp_call_frame(<2 x double> %values) {
entry:
  %cmp = fcmp ult <2 x double> %values, zeroinitializer
  br label %loop

loop:
  %keep_live = insertelement <2 x i1> %cmp, i1 false, i32 0
  br label %loop
}
