# RUN: split-file %s %t
# RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
# RUN:   %t/missing-name.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=MISSING-NAME
# RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
# RUN:   %t/unterminated.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNTERMINATED
# RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
# RUN:   %t/nested.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=NESTED
# RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
# RUN:   %t/zero-argument.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ZERO-ARGUMENT
# RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
# RUN:   %t/large-argument.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LARGE-ARGUMENT
# RUN: not llvm-mc -triple=tms9900 -tms9900-asm-dialect=xas99 \
# RUN:   %t/duplicate.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=DUPLICATE

# MISSING-NAME: error: expected macro name after .DEFM
#--- missing-name.s
        .DEFM

# UNTERMINATED: error: no matching .ENDM in macro definition
#--- unterminated.s
        .DEFM OPEN
        CLR #1

# NESTED: error: cannot define a macro within an xas99 macro
#--- nested.s
        .DEFM OUTER
        .DEFM INNER
        .ENDM
        .ENDM

# ZERO-ARGUMENT: error: xas99 macro argument must be in the range #1..#32
#--- zero-argument.s
        .DEFM ZERO
        CLR #0
        .ENDM

# LARGE-ARGUMENT: error: xas99 macro argument must be in the range #1..#32
#--- large-argument.s
        .DEFM LARGE
        CLR #33
        .ENDM

# DUPLICATE: error: macro '.SAME' is already defined
#--- duplicate.s
        .DEFM SAME
        CLR #1
        .ENDM
        .DEFM SAME
        INC #1
        .ENDM
