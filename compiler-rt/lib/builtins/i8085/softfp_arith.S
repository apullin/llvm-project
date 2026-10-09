; Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
; See https://llvm.org/LICENSE.txt for license information.
; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
; Binary32 arithmetic, round-to-nearest-even, using only 8080 instructions.
; Arguments: [entry SP+2..5], [entry SP+6..9]. Result: C:B:E:D.
; All workspace is on the caller's stack: reentrant, no static scratch.
;
; Frame (32 bytes): two unpacked operands at 0 and 8, each containing
; significand[3], sign, signed biased exponent[2], class, unused byte.
; Classes: normal/subnormal=0, zero=1, infinity=2, NaN=3.
; Result: extended significand[4] at 16, exponent[2] at 20, sign at 22.
; Byte 23 is a counter; 24..29 hold a full multiply product; 30 is a temp.
; The extended significand has three low guard/round/sticky bits.

        .macro FADDR offset
        lxi h, \offset
        dad sp
        .endm

        .macro FMANT offset
        FADDR \offset
        mov c, m
        inx h
        mov b, m
        inx h
        mov e, m
        .endm

        .macro FEXP_STORE
        FADDR 20
        mov m, e
        inx h
        mov m, d
        .endm

        .section .text.__i8085_fp_common, "ax", @progbits

; Unpack [HL]. Return significand C:B:E, sign D, signed exponent HL,
; class A. Subnormals are normalized without losing any significand bits.
.Lfp_unpack:
        mov c, m
        inx h
        mov b, m
        inx h
        mov e, m
        inx h
        mov d, m
        mov a, e
        rlc
        mov a, d
        ral
        mov l, a
        mvi h, 0
        mov a, d
        ani 128
        mov d, a
        mov a, e
        ani 127
        mov e, a
        mov a, l
        cpi 255
        jz .Lfp_unpack_special
        ora a
        jz .Lfp_unpack_subnormal
        mov a, e
        ori 128
        mov e, a
        xra a
        ret
.Lfp_unpack_special:
        mov a, c
        ora b
        ora e
        mvi a, 2
        rz
        mvi a, 3
        ret
.Lfp_unpack_subnormal:
        mov a, c
        ora b
        ora e
        mvi a, 1
        rz
        lxi h, 1
.Lfp_unpack_normalize:
        mov a, e
        ani 128
        jnz .Lfp_unpack_normalized
        mov a, c
        add a
        mov c, a
        mov a, b
        ral
        mov b, a
        mov a, e
        ral
        mov e, a
        dcx h
        jmp .Lfp_unpack_normalize
.Lfp_unpack_normalized:
        xra a
        ret

; Called with the 32-byte frame already allocated. Account for this
; helper's return address and the temporary PUSH of the unpacked exponent.
.Lfp_prepare:
        FADDR 36
        call .Lfp_unpack
        push h
        FADDR 4
        mov m, c
        inx h
        mov m, b
        inx h
        mov m, e
        inx h
        mov m, d
        inx h
        pop d
        mov m, e
        inx h
        mov m, d
        inx h
        mov m, a
        FADDR 40
        call .Lfp_unpack
        push h
        FADDR 12
        mov m, c
        inx h
        mov m, b
        inx h
        mov m, e
        inx h
        mov m, d
        inx h
        pop d
        mov m, e
        inx h
        mov m, d
        inx h
        mov m, a
        FADDR 5
        mov a, m
        FADDR 13
        xra m
        FADDR 24
        mov m, a
        ret

; Shift a four-byte little-endian value at HL right B times, jamming every
; discarded one into bit zero. DE/C are preserved, HL remains the base.
.Lfp_shr_jam:
        mov a, b
        ora a
        rz
.Lfp_shr_loop:
        inx h
        inx h
        inx h
        mov a, m
        ora a
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        jnc .Lfp_shr_no_jam
        ori 1
        mov m, a
.Lfp_shr_no_jam:
        dcr b
        jnz .Lfp_shr_loop
        ret

; Shift the four-byte value at HL left B times; preserve DE/C and HL.
.Lfp_shl:
        mov a, b
        ora a
        rz
.Lfp_shl_loop:
        mov a, m
        add a
        mov m, a
        inx h
        mov a, m
        ral
        mov m, a
        inx h
        mov a, m
        ral
        mov m, a
        inx h
        mov a, m
        ral
        mov m, a
        dcx h
        dcx h
        dcx h
        dcr b
        jnz .Lfp_shl_loop
        ret

; Copy a three-byte significand from HL to DE, append zero, and extend
; by three bits. Returns HL pointing to the extended destination.
.Lfp_copy_sig:
        mvi b, 3
.Lfp_copy_sig_loop:
        mov a, m
        stax d
        inx h
        inx d
        dcr b
        jnz .Lfp_copy_sig_loop
        xra a
        stax d
        xchg
        dcx h
        dcx h
        dcx h
        mvi b, 3
        jmp .Lfp_shl

.Lfp_inc_exp:
        FADDR 22
        inr m
        rnz
        inx h
        inr m
        ret
.Lfp_dec_exp:
        FADDR 22
        mov a, m
        sui 1
        mov m, a
        inx h
        mov a, m
        sbi 0
        mov m, a
        ret

; Called once from an arithmetic entry. Normalize-to-subnormal uses the
; same sticky shift as alignment. Rounding can promote to the smallest
; normal, carry into the next exponent, or overflow to infinity.
.Lfp_round_pack:
        FADDR 22
        mov e, m
        inx h
        mov d, m
        mov a, d
        ora a
        jm .Lfp_pack_subnormal
        jnz .Lfp_pack_inf
        mov a, e
        ora a
        jz .Lfp_pack_subnormal
        cpi 255
        jnc .Lfp_pack_inf
        jmp .Lfp_pack_round
.Lfp_pack_subnormal:
        mov a, d
        ora a
        jz .Lfp_pack_shift
        cpi 255
        jnz .Lfp_pack_zero
        mov a, e
        cpi 230
        jc .Lfp_pack_zero
.Lfp_pack_shift:
        mvi a, 1
        sub e
        mov b, a
        FADDR 18
        call .Lfp_shr_jam
        FADDR 22
        mvi m, 0
        inx h
        mvi m, 0
.Lfp_pack_round:
        FADDR 18
        mov c, m
        inx h
        mov b, m
        inx h
        mov e, m
        inx h
        mov d, m
        mov a, c
        ani 7
        mov l, a
        mvi h, 3
.Lfp_pack_drop_extra:
        mov a, d
        ora a
        rar
        mov d, a
        mov a, e
        rar
        mov e, a
        mov a, b
        rar
        mov b, a
        mov a, c
        rar
        mov c, a
        dcr h
        jnz .Lfp_pack_drop_extra
        mov a, l
        cpi 4
        jc .Lfp_pack_no_increment
        jnz .Lfp_pack_increment
        mov a, c
        ani 1
        jz .Lfp_pack_no_increment
.Lfp_pack_increment:
        mov a, c
        adi 1
        mov c, a
        mov a, b
        aci 0
        mov b, a
        mov a, e
        aci 0
        mov e, a
        mov a, d
        aci 0
        mov d, a
.Lfp_pack_no_increment:
        mov a, d
        ora a
        jz .Lfp_pack_exponent
        lxi b, 0
        mvi e, 128
        FADDR 22
        inr m
.Lfp_pack_exponent:
        FADDR 22
        mov d, m
        mov a, d
        ora a
        jnz .Lfp_pack_encode
        mov a, e
        ani 128
        jz .Lfp_pack_encode
        inr d
.Lfp_pack_encode:
        mov a, d
        ani 1
        rrc
        mov l, a
        mov a, e
        ani 127
        ora l
        mov e, a
        mov a, d
        ora a
        rar
        mov d, a
        FADDR 24
        mov a, m
        ora d
        mov d, a
        ret
.Lfp_pack_inf:
        FADDR 24
        mov a, m
        ori 127
        mov d, a
        mvi e, 128
        lxi b, 0
        ret
.Lfp_pack_zero:
        FADDR 24
        mov d, m
        mvi e, 0
        lxi b, 0
        ret

; The following continuations run at the entry's frame SP, not a helper SP.
.Lfp_nan:
        lxi b, 0
        lxi d, 32704
        jmp .Lfp_finish
.Lfp_inf:
        FADDR 22
        mov a, m
        ori 127
        mov d, a
        mvi e, 128
        lxi b, 0
        jmp .Lfp_finish
.Lfp_zero:
        FADDR 22
        mov d, m
        mvi e, 0
        lxi b, 0
        jmp .Lfp_finish
.Lfp_return_a:
        FADDR 34
        jmp .Lfp_return_arg
.Lfp_return_b:
        FADDR 38
.Lfp_return_arg:
        mov c, m
        inx h
        mov b, m
        inx h
        mov e, m
        inx h
        mov d, m
        jmp .Lfp_finish
.Lfp_pack:
        call .Lfp_round_pack
.Lfp_finish:
        FADDR 32
        sphl
        ret

        .section .text.__addsf3, "ax", @progbits
        .globl __addsf3
        .type __addsf3,@function
__addsf3:
        FADDR -32
        sphl
        call .Lfp_prepare
        FADDR 6
        mov b, m
        FADDR 14
        mov c, m
        mov a, b
        cpi 3
        jz .Lfp_nan
        mov a, c
        cpi 3
        jz .Lfp_nan
        mov a, b
        cpi 2
        jnz .Lfp_add_check_b_inf
        mov a, c
        cpi 2
        jnz .Lfp_return_a
        FADDR 22
        mov a, m
        ora a
        jnz .Lfp_nan
        jmp .Lfp_return_a
.Lfp_add_check_b_inf:
        mov a, c
        cpi 2
        jz .Lfp_return_b
        mov a, b
        cpi 1
        jnz .Lfp_add_check_b_zero
        mov a, c
        cpi 1
        jnz .Lfp_return_b
        FADDR 3
        mov a, m
        FADDR 11
        ana m
        FADDR 22
        mov m, a
        jmp .Lfp_zero
.Lfp_add_check_b_zero:
        mov a, c
        cpi 1
        jz .Lfp_return_a

        FADDR 4
        mov e, m
        inx h
        mov d, m
        FADDR 12
        mov c, m
        inx h
        mov b, m
        mov a, e
        sub c
        mov e, a
        mov a, d
        sbb b
        mov d, a
        ora a
        jp .Lfp_add_ordered
        mov a, e
        cma
        adi 1
        mov e, a
        mov a, d
        cma
        aci 0
        mov d, a
        push d
        FADDR 10
        xchg
        FADDR 2
        mvi b, 8
.Lfp_add_swap:
        mov c, m
        ldax d
        mov m, a
        mov a, c
        stax d
        inx h
        inx d
        dcr b
        jnz .Lfp_add_swap
        pop d
.Lfp_add_ordered:
        mov a, d
        ora a
        mvi a, 31
        jnz .Lfp_add_distance
        mov a, e
        cpi 32
        jc .Lfp_add_distance
        mvi a, 31
.Lfp_add_distance:
        FADDR 23
        mov m, a
        FADDR 22
        mov a, m
        FADDR 30
        mov m, a
        FADDR 3
        mov a, m
        FADDR 22
        mov m, a
        FADDR 4
        mov e, m
        inx h
        mov d, m
        FEXP_STORE
        FADDR 16
        xchg
        FADDR 0
        call .Lfp_copy_sig
        FADDR 11
        mvi m, 0
        FADDR 8
        mvi b, 3
        call .Lfp_shl
        FADDR 23
        mov b, m
        FADDR 8
        call .Lfp_shr_jam
        FADDR 30
        mov a, m
        ora a
        jnz .Lfp_add_subtract
        FADDR 8
        xchg
        FADDR 16
        mvi b, 4
        xra a
.Lfp_add_sum_loop:
        ldax d
        mov c, a
        mov a, m
        adc c
        mov m, a
        inx h
        inx d
        dcr b
        jnz .Lfp_add_sum_loop
        dcx h
        mov a, m
        ani 8
        jz .Lfp_pack
        FADDR 16
        mvi b, 1
        call .Lfp_shr_jam
        call .Lfp_inc_exp
        jmp .Lfp_pack
.Lfp_add_subtract:
        FADDR 8
        xchg
        FADDR 16
        mvi b, 4
        xra a
.Lfp_add_difference_loop:
        ldax d
        mov c, a
        mov a, m
        sbb c
        mov m, a
        inx h
        inx d
        dcr b
        jnz .Lfp_add_difference_loop
        jnc .Lfp_add_positive_difference
        FADDR 22
        mov a, m
        xri 128
        mov m, a
        FADDR 16
        mvi b, 4
        stc
.Lfp_add_negate:
        mov a, m
        cma
        aci 0
        mov m, a
        inx h
        dcr b
        jnz .Lfp_add_negate
.Lfp_add_positive_difference:
        FADDR 16
        mov a, m
        inx h
        ora m
        inx h
        ora m
        inx h
        ora m
        jnz .Lfp_add_normalize
        FADDR 22
        mvi m, 0
        jmp .Lfp_zero
.Lfp_add_normalize:
        FADDR 19
        mov a, m
        ani 4
        jnz .Lfp_pack
        FADDR 16
        mvi b, 1
        call .Lfp_shl
        call .Lfp_dec_exp
        jmp .Lfp_add_normalize
        .size __addsf3, .-__addsf3

        .section .text.__mulsf3, "ax", @progbits
        .globl __mulsf3
        .type __mulsf3,@function
__mulsf3:
        FADDR -32
        sphl
        call .Lfp_prepare
        FADDR 6
        mov b, m
        FADDR 14
        mov c, m
        mov a, b
        cpi 3
        jz .Lfp_nan
        mov a, c
        cpi 3
        jz .Lfp_nan
        mov a, b
        cpi 2
        jnz .Lfp_mul_check_b_inf
        mov a, c
        cpi 1
        jz .Lfp_nan
        jmp .Lfp_inf
.Lfp_mul_check_b_inf:
        mov a, c
        cpi 2
        jnz .Lfp_mul_check_zero
        mov a, b
        cpi 1
        jz .Lfp_nan
        jmp .Lfp_inf
.Lfp_mul_check_zero:
        mov a, b
        cpi 1
        jz .Lfp_zero
        mov a, c
        cpi 1
        jz .Lfp_zero
        FADDR 4
        mov e, m
        inx h
        mov d, m
        FADDR 12
        mov c, m
        inx h
        mov b, m
        mov a, e
        add c
        mov e, a
        mov a, d
        adc b
        mov d, a
        mov a, e
        sui 127
        mov e, a
        mov a, d
        sbi 0
        mov d, a
        FEXP_STORE
        FADDR 24
        mvi b, 6
        xra a
.Lfp_mul_clear:
        mov m, a
        inx h
        dcr b
        jnz .Lfp_mul_clear
        FADDR 23
        mvi m, 24
; Before the first set multiplier bit, every product shift is a shift of
; zero. Skip whole zero bytes, then the remaining zero bits, without touching
; the product. The remaining bit count still produces the full 48-bit product.
.Lfp_mul_skip_byte:
        FADDR 8
        mov a, m
        ora a
        jnz .Lfp_mul_skip_bit
        inx h
        mov a, m
        dcx h
        mov m, a
        inx h
        inx h
        mov a, m
        dcx h
        mov m, a
        inx h
        mvi m, 0
        FADDR 23
        mov a, m
        sui 8
        mov m, a
        jmp .Lfp_mul_skip_byte
.Lfp_mul_skip_bit:
        FADDR 8
        mov a, m
        ani 1
        jnz .Lfp_mul_loop
        inx h
        inx h
        mov a, m
        ora a
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        FADDR 23
        dcr m
        jmp .Lfp_mul_skip_bit
.Lfp_mul_loop:
        FADDR 8
        mov a, m
        ani 1
        jz .Lfp_mul_no_add
        FMANT 0
        FADDR 27
        mov a, m
        add c
        mov m, a
        inx h
        mov a, m
        adc b
        mov m, a
        inx h
        mov a, m
        adc e
        mov m, a
        jmp .Lfp_mul_shift
.Lfp_mul_no_add:
        FADDR 29
        mov a, m
        ora a
.Lfp_mul_shift:
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        FADDR 10
        mov a, m
        ora a
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        dcx h
        mov a, m
        rar
        mov m, a
        FADDR 23
        dcr m
        jnz .Lfp_mul_loop
        FADDR 29
        mov a, m
        ani 128
        mvi a, 4
        jz .Lfp_mul_distance
        call .Lfp_inc_exp
        mvi a, 5
.Lfp_mul_distance:
        FADDR 23
        mov m, a
        FADDR 16
        xchg
        FADDR 26
        mvi b, 4
.Lfp_mul_copy:
        mov a, m
        stax d
        inx h
        inx d
        dcr b
        jnz .Lfp_mul_copy
        FADDR 24
        mov a, m
        inx h
        ora m
        jz .Lfp_mul_jammed
        FADDR 16
        mov a, m
        ori 1
        mov m, a
.Lfp_mul_jammed:
        FADDR 23
        mov b, m
        FADDR 16
        call .Lfp_shr_jam
        jmp .Lfp_pack
        .size __mulsf3, .-__mulsf3

        .section .text.__divsf3, "ax", @progbits
        .globl __divsf3
        .type __divsf3,@function
__divsf3:
        FADDR -32
        sphl
        call .Lfp_prepare
        FADDR 6
        mov b, m
        FADDR 14
        mov c, m
        mov a, b
        cpi 3
        jz .Lfp_nan
        mov a, c
        cpi 3
        jz .Lfp_nan
        mov a, b
        cpi 2
        jnz .Lfp_div_check_b_inf
        mov a, c
        cpi 2
        jz .Lfp_nan
        jmp .Lfp_inf
.Lfp_div_check_b_inf:
        mov a, c
        cpi 2
        jz .Lfp_zero
        mov a, b
        cpi 1
        jnz .Lfp_div_check_b_zero
        mov a, c
        cpi 1
        jz .Lfp_nan
        jmp .Lfp_zero
.Lfp_div_check_b_zero:
        mov a, c
        cpi 1
        jz .Lfp_inf
        FADDR 4
        mov e, m
        inx h
        mov d, m
        FADDR 12
        mov c, m
        inx h
        mov b, m
        mov a, e
        sub c
        mov e, a
        mov a, d
        sbb b
        mov d, a
        mov a, e
        adi 127
        mov e, a
        mov a, d
        aci 0
        mov d, a
        FEXP_STORE
        FADDR 3
        mvi m, 0
        FMANT 0
        FADDR 10
        mov a, e
        cmp m
        jnz .Lfp_div_ordered
        dcx h
        mov a, b
        cmp m
        jnz .Lfp_div_ordered
        dcx h
        mov a, c
        cmp m
.Lfp_div_ordered:
        jnc .Lfp_div_normalized
        FADDR 0
        mvi b, 1
        call .Lfp_shl
        call .Lfp_dec_exp
.Lfp_div_normalized:
        FADDR 16
        mvi b, 4
        xra a
.Lfp_div_clear:
        mov m, a
        inx h
        dcr b
        jnz .Lfp_div_clear
        FADDR 23
        mvi m, 27
.Lfp_div_loop:
        FADDR 3
        mov a, m
        ora a
        jnz .Lfp_div_subtract
        FMANT 0
        FADDR 10
        mov a, e
        cmp m
        jnz .Lfp_div_compare
        dcx h
        mov a, b
        cmp m
        jnz .Lfp_div_compare
        dcx h
        mov a, c
        cmp m
.Lfp_div_compare:
        jc .Lfp_div_zero_bit
.Lfp_div_subtract:
        FMANT 8
        FADDR 0
        mov a, m
        sub c
        mov m, a
        inx h
        mov a, m
        sbb b
        mov m, a
        inx h
        mov a, m
        sbb e
        mov m, a
        inx h
        mov a, m
        sbi 0
        mov m, a
        mvi e, 1
        jmp .Lfp_div_quotient
.Lfp_div_zero_bit:
        mvi e, 0
.Lfp_div_quotient:
        FADDR 16
        mov a, e
        rar
        mov a, m
        ral
        mov m, a
        inx h
        mov a, m
        ral
        mov m, a
        inx h
        mov a, m
        ral
        mov m, a
        inx h
        mov a, m
        ral
        mov m, a
        FADDR 23
        dcr m
        jz .Lfp_div_done
        FADDR 0
        mov a, m
        inx h
        ora m
        inx h
        ora m
        inx h
        ora m
        jnz .Lfp_div_more_bits
; An exact remainder leaves only zero quotient bits. Materialize those
; bits directly, including the three rounding bits, instead of dividing them.
        FADDR 23
        mov b, m
        FADDR 16
        call .Lfp_shl
        jmp .Lfp_pack
.Lfp_div_more_bits:
        FADDR 0
        mvi b, 1
        call .Lfp_shl
        jmp .Lfp_div_loop
.Lfp_div_done:
        FADDR 0
        mov a, m
        inx h
        ora m
        inx h
        ora m
        inx h
        ora m
        jz .Lfp_pack
        FADDR 16
        mov a, m
        ori 1
        mov m, a
        jmp .Lfp_pack
        .size __divsf3, .-__divsf3

        .purgem FADDR
        .purgem FMANT
        .purgem FEXP_STORE
