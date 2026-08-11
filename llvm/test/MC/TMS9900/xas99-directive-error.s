; RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
; RUN:   -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

        DATA >1234 trailing

; CHECK: error: unexpected token in DATA directive
