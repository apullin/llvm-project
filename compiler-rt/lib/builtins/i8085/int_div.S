; Hand-optimised i8085 integer division/remainder routines.
;
; Replaces the C implementations that promote everything to i32 and
; use a 32-iteration division loop even for i8 and i16.
;
; Calling convention (stack-based, cdecl):
;   Arguments pushed right-to-left.  Each arg occupies its natural
;   size on the stack (uint8_t = 1 byte, uint16_t = 2, uint32_t = 4).
;   8-bit return in A.  16-bit return in BC.  32-bit return in BC:DE
;   (C = byte 0 LSB, B = byte 1, E = byte 2, D = byte 3 MSB).
;
; Algorithm: restoring division (MSB-first).
;   quotient = 0
;   remainder = 0
;   for each bit from MSB to LSB:
;     remainder = (remainder << 1) | MSB(dividend)
;     dividend <<= 1
;     if remainder >= divisor:
;       remainder -= divisor
;       quotient = (quotient << 1) | 1
;     else:
;       quotient <<= 1


; ===================================================================
; __udivmod8: unsigned 8-bit divmod
;   [SP+2] = dividend (u8)
;   [SP+3] = divisor (u8)
;   Returns: A = quotient, C = remainder
;
; Registers during loop:
;   B = dividend (shifts left, MSB extracted each iteration)
;   C = remainder (accumulates)
;   D = divisor (constant)
;   E = quotient (builds up)
;   L = loop counter
; ===================================================================
	.section .text.__udivmod8, "ax", @progbits
	.globl	__udivmod8
	.type	__udivmod8,@function
__udivmod8:
	; Load dividend into B
	lxi	h, 2
	dad	sp
	mov	b, m

	; Load divisor into D
	lxi	h, 3
	dad	sp
	mov	d, m

	; Check for divide by zero
	mov	a, d
	ora	a
	jnz	.Ludm8_start

	; Division by zero: return 0 quotient, 0 remainder
	xra	a
	mvi	c, 0
	ret

.Ludm8_start:
	; Initialize: remainder = 0, quotient = 0, counter = 8
	mvi	c, 0		; remainder
	mvi	e, 0		; quotient
	mvi	l, 8		; loop counter

.Ludm8_loop:
	; remainder = (remainder << 1) | MSB(dividend)
	; First shift dividend left, MSB goes into carry
	mov	a, b
	add	a		; B <<= 1, carry = old MSB
	mov	b, a

	; Shift remainder left and bring in carry (MSB of dividend)
	mov	a, c
	adc	a		; C = C*2 + carry
	mov	c, a

	; Shift quotient left (make room for new bit)
	mov	a, e
	add	a		; E <<= 1
	mov	e, a

	; Compare: remainder >= divisor?
	mov	a, c
	sub	d		; A = remainder - divisor
	jc	.Ludm8_no_sub	; if remainder < divisor, skip

	; remainder -= divisor (A already has the result)
	mov	c, a
	; Set bit 0 of quotient
	inr	e

.Ludm8_no_sub:
	dcr	l		; decrement counter
	jnz	.Ludm8_loop

	; Return: A = quotient, C = remainder
	mov	a, e
	ret
	.size	__udivmod8, .-__udivmod8


; ===================================================================
; __udiv8: unsigned 8-bit divide
;   [SP+2] = dividend (u8)
;   [SP+3] = divisor (u8)
;   Returns: A = quotient (also C = quotient, B = 0 for i16 compat)
;
; Tail-call: JMP (not CALL) so __udivmod8 sees the same stack frame.
; __udivmod8 returns A=quotient, C=remainder.
; ===================================================================
	.section .text.__udiv8, "ax", @progbits
	.globl	__udiv8
	.type	__udiv8,@function
__udiv8:
	jmp	__udivmod8
	; __udivmod8 returns directly to our caller with A=quotient
	.size	__udiv8, .-__udiv8


; ===================================================================
; __urem8: unsigned 8-bit remainder
;   [SP+2] = dividend (u8)
;   [SP+3] = divisor (u8)
;   Returns: A = remainder (also C = remainder, B = 0 for i16 compat)
;
; Load args and re-push so __udivmod8 sees them at the right offset.
; ===================================================================
	.section .text.__urem8, "ax", @progbits
	.globl	__urem8
	.type	__urem8,@function
__urem8:
	; Load args from our stack frame
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	b, l		; B = dividend
	mov	d, h		; D = divisor
#else
	lxi	h, 2
	dad	sp
	mov	b, m		; B = dividend
	inx	h
	mov	d, m		; D = divisor
#endif

	; Push as args for __udivmod8
	; __udivmod8 expects [SP+2]=dividend, [SP+3]=divisor
	; After push: [SP+0..1]=pushed data, [SP+2..3]=our ret addr, ...
	; We push one byte for divisor and one for dividend? No, push is 16-bit.
	; Simpler: push both as a 16-bit word.
	; We need [SP+2]=dividend=B, [SP+3]=divisor=D after the call.
	; CALL will push 2 bytes of return address at [SP+0..1].
	; So we need dividend at [SP+2], divisor at [SP+3].
	; Push D (high) and B (low) as a word: push with D at [SP+1], B at [SP+0]
	; Then CALL pushes ret addr at [SP-2..SP-1], new [SP+2..3] = B, D. Correct!
	mov	c, b		; C = dividend (low of BC push)
	mov	b, d		; B = divisor (high of BC push)
	push	b		; [SP+0]=C=dividend, [SP+1]=B=divisor

	call	__udivmod8

	; Clean up pushed args (2 bytes)
	pop	h		; discard

	; A = quotient (not needed), C = remainder
	mov	a, c
	mvi	b, 0
	ret
	.size	__urem8, .-__urem8


; ===================================================================
; __sdiv8: signed 8-bit divide
;   [SP+2] = dividend (s8)
;   [SP+3] = divisor (s8)
;   Returns: A = quotient (also C = quotient, B = 0/0xFF for i16 compat)
;
; Strategy: determine result sign, negate inputs if needed,
;           call __udivmod8, fix sign of quotient.
; ===================================================================
	.section .text.__sdiv8, "ax", @progbits
	.globl	__sdiv8
	.type	__sdiv8,@function
__sdiv8:
	; Load dividend
	lxi	h, 2
	dad	sp
	mov	b, m		; B = dividend

	; Load divisor
	lxi	h, 3
	dad	sp
	mov	d, m		; D = divisor

	; Compute result sign: (dividend ^ divisor) & 0x80
	mov	a, b
	xra	d
	push	psw		; save sign on stack

	; Make dividend unsigned
	mov	a, b
	ora	a
	jp	.Lsd8_a_pos
	cma
	adi	1
	mov	b, a
.Lsd8_a_pos:

	; Make divisor unsigned
	mov	a, d
	ora	a
	jp	.Lsd8_b_pos
	cma
	adi	1
	mov	d, a
.Lsd8_b_pos:

	; Push unsigned args and call __udivmod8
	; We already have B=|dividend|, D=|divisor| in registers.
	; But __udivmod8 reads from the stack at [SP+2] and [SP+3].
	; We need to push these values. The return address from our
	; CALL is already on the stack, plus we pushed PSW.
	; Stack: [SP+0..1] = sign, [SP+2..3] = ret addr, [SP+4] = orig_dividend, [SP+5] = orig_divisor
	;
	; We'll call into the udivmod8 core directly with inline code
	; since we already have the values in registers.

	; Check for divide by zero
	mov	a, d
	ora	a
	jnz	.Lsd8_start
	; Division by zero
	pop	psw
	xra	a
	mvi	c, 0
	mvi	b, 0
	ret

.Lsd8_start:
	mvi	c, 0		; remainder
	mvi	e, 0		; quotient
	mvi	l, 8		; counter

.Lsd8_loop:
	mov	a, b
	add	a
	mov	b, a
	mov	a, c
	adc	a
	mov	c, a
	mov	a, e
	add	a
	mov	e, a
	mov	a, c
	sub	d
	jc	.Lsd8_no_sub
	mov	c, a
	inr	e
.Lsd8_no_sub:
	dcr	l
	jnz	.Lsd8_loop

	; E = quotient (unsigned)
	; Pop sign flag
	pop	psw
	ora	a		; test bit 7
	jp	.Lsd8_pos_result

	; Negate quotient
	mov	a, e
	cma
	adi	1
	mov	c, a
	; Sign-extend to BC
	mov	a, c
	ral			; carry = sign bit
	sbb	a		; A = 0xFF if negative, 0x00 if positive
	mov	b, a
	mov	a, c
	ret

.Lsd8_pos_result:
	mov	a, e
	mov	c, a
	mvi	b, 0
	ret
	.size	__sdiv8, .-__sdiv8


; ===================================================================
; __srem8: signed 8-bit remainder
;   [SP+2] = dividend (s8)
;   [SP+3] = divisor (s8)
;   Returns: A = remainder (sign follows dividend)
;
; Strategy: same as __sdiv8 but return remainder with dividend's sign.
; ===================================================================
	.section .text.__srem8, "ax", @progbits
	.globl	__srem8
	.type	__srem8,@function
__srem8:
	; Load dividend
	lxi	h, 2
	dad	sp
	mov	b, m

	; Load divisor
	lxi	h, 3
	dad	sp
	mov	d, m

	; Save dividend sign (remainder sign = dividend sign)
	mov	a, b
	push	psw

	; Make dividend unsigned
	mov	a, b
	ora	a
	jp	.Lsr8_a_pos
	cma
	adi	1
	mov	b, a
.Lsr8_a_pos:

	; Make divisor unsigned
	mov	a, d
	ora	a
	jp	.Lsr8_b_pos
	cma
	adi	1
	mov	d, a
.Lsr8_b_pos:

	; Check for divide by zero
	mov	a, d
	ora	a
	jnz	.Lsr8_start
	pop	psw
	xra	a
	mvi	c, 0
	mvi	b, 0
	ret

.Lsr8_start:
	mvi	c, 0		; remainder
	mvi	e, 0		; quotient (not used but needed for algorithm)
	mvi	l, 8		; counter

.Lsr8_loop:
	mov	a, b
	add	a
	mov	b, a
	mov	a, c
	adc	a
	mov	c, a
	mov	a, e
	add	a
	mov	e, a
	mov	a, c
	sub	d
	jc	.Lsr8_no_sub
	mov	c, a
	inr	e
.Lsr8_no_sub:
	dcr	l
	jnz	.Lsr8_loop

	; C = remainder (unsigned)
	; Pop dividend sign
	pop	psw
	ora	a		; test bit 7
	jp	.Lsr8_pos_result

	; Negate remainder
	mov	a, c
	cma
	adi	1
	mov	c, a
	mov	a, c
	ral
	sbb	a
	mov	b, a
	mov	a, c
	ret

.Lsr8_pos_result:
	mov	a, c
	mvi	b, 0
	ret
	.size	__srem8, .-__srem8


; ===================================================================
; __sdivmod8: signed 8-bit divmod
;   [SP+2] = dividend (s8)
;   [SP+3] = divisor (s8)
;   Returns: A = quotient, C = remainder
;
; Strategy: save both signs, take absolute values, call __udivmod8
;           core, fix sign of quotient (dividend_sign XOR divisor_sign)
;           and remainder (dividend_sign).
;
; Stack usage: 2 bytes (two sign bytes pushed)
; ===================================================================
	.section .text.__sdivmod8, "ax", @progbits
	.globl	__sdivmod8
	.type	__sdivmod8,@function
__sdivmod8:
	; Load dividend
	lxi	h, 2
	dad	sp
	mov	b, m		; B = dividend

	; Load divisor
	lxi	h, 3
	dad	sp
	mov	d, m		; D = divisor

	; Save dividend sign (for remainder sign)
	mov	a, b
	push	psw		; [SP+0..1] = dividend sign

	; Compute quotient sign: (dividend ^ divisor) & 0x80
	mov	a, b
	xra	d
	push	psw		; [SP+0..1] = quotient sign

	; Make dividend unsigned
	mov	a, b
	ora	a
	jp	.Lsdm8_a_pos
	cma
	adi	1
	mov	b, a
.Lsdm8_a_pos:

	; Make divisor unsigned
	mov	a, d
	ora	a
	jp	.Lsdm8_b_pos
	cma
	adi	1
	mov	d, a
.Lsdm8_b_pos:

	; Check for divide by zero
	mov	a, d
	ora	a
	jnz	.Lsdm8_start
	; Division by zero
	pop	psw
	pop	psw
	xra	a
	mvi	c, 0
	ret

.Lsdm8_start:
	; Inline the unsigned divmod loop (same as __udivmod8 core)
	mvi	c, 0		; remainder
	mvi	e, 0		; quotient
	mvi	l, 8		; counter

.Lsdm8_loop:
	mov	a, b
	add	a
	mov	b, a
	mov	a, c
	adc	a
	mov	c, a
	mov	a, e
	add	a
	mov	e, a
	mov	a, c
	sub	d
	jc	.Lsdm8_no_sub
	mov	c, a
	inr	e
.Lsdm8_no_sub:
	dcr	l
	jnz	.Lsdm8_loop

	; E = unsigned quotient, C = unsigned remainder

	; Fix quotient sign
	pop	psw		; quotient sign flag
	ora	a		; test bit 7
	jp	.Lsdm8_qpos

	; Negate quotient
	mov	a, e
	cma
	adi	1
	mov	e, a
.Lsdm8_qpos:

	; Fix remainder sign (remainder sign = dividend sign)
	pop	psw		; dividend sign flag
	ora	a		; test bit 7
	jp	.Lsdm8_rpos

	; Negate remainder
	mov	a, c
	cma
	adi	1
	mov	c, a
.Lsdm8_rpos:

	; Return: A = quotient, C = remainder
	mov	a, e
	ret
	.size	__sdivmod8, .-__sdivmod8


; ===================================================================
; __udivmod16: unsigned 16-bit divmod
;   [SP+2..3] = dividend (u16, little-endian)
;   [SP+4..5] = divisor (u16, little-endian)
;   Returns: BC = quotient, DE = remainder
;
; Registers during loop:
;   Stack [SP+0..1] = dividend (shifts left)
;   DE = remainder
;   BC = quotient (builds up)
;   Stack [SP+2..3] = divisor (constant, read from orig args)
;   L = loop counter
;
; We use 4 bytes of stack for dividend + counter.
; ===================================================================
	.section .text.__udivmod16, "ax", @progbits
	.globl	__udivmod16
	.type	__udivmod16,@function
__udivmod16:
	; Load dividend into BC (temporarily)
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	c, l
	mov	b, h
#else
	lxi	h, 2
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
#endif

	; Load divisor into DE (temporarily to check for zero)
#ifdef UNDOC
	ldsi	4
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 4
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Check for divide by zero
	mov	a, d
	ora	e
	jnz	.Ludm16_start

	; Division by zero
	lxi	b, 0
	lxi	d, 0
	ret

.Ludm16_start:
	; Push dividend onto stack for shifting
	push	b		; [SP+0..1] = dividend (shifts left)

	; Stack layout:
	;   [SP+0..1] = dividend
	;   [SP+2..3] = return address
	;   [SP+4..5] = arg: dividend (original)
	;   [SP+6..7] = arg: divisor

	; Initialize: quotient BC = 0, remainder DE = 0
	lxi	b, 0
	lxi	d, 0
	mvi	l, 16		; loop counter (we'll save/restore HL)

.Ludm16_loop:
	push	h		; save counter (SP shifts +2)

	; Shift dividend left by 1 bit, MSB into carry
	; dividend is at [SP+2] (shifted by push h)
	lxi	h, 2
	dad	sp
	mov	a, m		; dividend low
	add	a		; shift left, carry = old bit 7
	mov	m, a
	inx	h
	mov	a, m		; dividend high
	adc	a		; shift left with carry
	mov	m, a
	; Carry now holds MSB of dividend

	; Shift remainder (DE) left and bring in carry
	; DE <<= 1, bit 0 = carry from dividend
	mov	a, e
	adc	a		; E = E*2 + carry
	mov	e, a
	mov	a, d
	adc	a		; D = D*2 + carry from E
	mov	d, a

	; Shift quotient (BC) left, make room for new bit
	mov	a, c
	add	a		; C <<= 1
	mov	c, a
	mov	a, b
	adc	a		; B <<= 1 with carry
	mov	b, a

	; Compare remainder (DE) >= divisor
	; Divisor is at [SP+8] (shifted by push dividend + push counter)
	lxi	h, 8
	dad	sp
	; Compare DE vs [HL..HL+1]
	; Compare high bytes first
	mov	a, d
	cmp	m		; compare D (rem_hi) vs divisor_lo... wait
	; Actually divisor at [SP+8..9]: [SP+8]=low, [SP+9]=high (little-endian)
	; We need to compare DE (D=high, E=low) vs divisor (high at [SP+9], low at [SP+8])
	inx	h		; HL -> divisor high
	mov	a, d
	cmp	m		; D vs divisor_high
	jc	.Ludm16_no_sub	; if D < divisor_high, skip
	jnz	.Ludm16_do_sub	; if D > divisor_high, definitely subtract
	; High bytes equal, compare low bytes
	dcx	h		; HL -> divisor low
	mov	a, e
	cmp	m		; E vs divisor_low
	jc	.Ludm16_no_sub	; if E < divisor_low, skip

.Ludm16_do_sub:
	; remainder -= divisor
	; Divisor at [SP+8..9]
	lxi	h, 8
	dad	sp
	mov	a, e
	sub	m		; E -= divisor_low
	mov	e, a
	inx	h
	mov	a, d
	sbb	m		; D -= divisor_high - borrow
	mov	d, a
	; Set bit 0 of quotient
	inr	c

.Ludm16_no_sub:
	pop	h		; restore counter
	dcr	l		; decrement
	jnz	.Ludm16_loop

	; Deallocate dividend from stack (2 bytes)
	pop	h		; discard dividend

	; BC = quotient, DE = remainder
	ret
	.size	__udivmod16, .-__udivmod16


; ===================================================================
; __udiv16: unsigned 16-bit divide
;   [SP+2..3] = dividend (u16)
;   [SP+4..5] = divisor (u16)
;   Returns: BC = quotient
;
; Tail-call: JMP so __udivmod16 sees the same stack frame.
; ===================================================================
	.section .text.__udiv16, "ax", @progbits
	.globl	__udiv16
	.type	__udiv16,@function
__udiv16:
	jmp	__udivmod16
	; __udivmod16 returns directly to our caller with BC=quotient
	.size	__udiv16, .-__udiv16


; ===================================================================
; __urem16: unsigned 16-bit remainder
;   [SP+2..3] = dividend (u16)
;   [SP+4..5] = divisor (u16)
;   Returns: BC = remainder
;
; Load and re-push args for __udivmod16.
; ===================================================================
	.section .text.__urem16, "ax", @progbits
	.globl	__urem16
	.type	__urem16,@function
__urem16:
	; Load args from our stack frame
	lxi	h, 2
	dad	sp
	mov	c, m		; dividend low
	inx	h
	mov	b, m		; dividend high
	inx	h
	mov	e, m		; divisor low
	inx	h
	mov	d, m		; divisor high

	; Push divisor then dividend (right-to-left for cdecl)
	push	d		; divisor
	push	b		; dividend

	call	__udivmod16

	; Clean up 4 bytes of pushed args
	pop	h		; discard
	pop	h		; discard

	; DE = remainder, move to BC
	mov	b, d
	mov	c, e
	ret
	.size	__urem16, .-__urem16


; ===================================================================
; __sdiv16: signed 16-bit divide
;   [SP+2..3] = dividend (s16)
;   [SP+4..5] = divisor (s16)
;   Returns: BC = quotient
;
; Strategy: determine result sign, negate inputs, unsigned divide,
;           fix sign.
; ===================================================================
	.section .text.__sdiv16, "ax", @progbits
	.globl	__sdiv16
	.type	__sdiv16,@function
__sdiv16:
	; Load dividend into BC
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	c, l
	mov	b, h
#else
	lxi	h, 2
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
#endif

	; Load divisor into DE
#ifdef UNDOC
	ldsi	4
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 4
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Compute result sign: XOR of high bytes
	mov	a, b
	xra	d
	push	psw		; save sign flag

	; Make dividend (BC) unsigned
	mov	a, b
	ora	a
	jp	.Lsd16_a_pos
	; Negate BC
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lsd16_a_pos:

	; Make divisor (DE) unsigned
	mov	a, d
	ora	a
	jp	.Lsd16_b_pos
	; Negate DE
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lsd16_b_pos:

	; Push unsigned args for __udivmod16
	; __udivmod16 expects [SP+2..3]=dividend, [SP+4..5]=divisor
	; Current stack: [SP+0..1]=sign, [SP+2..3]=ret addr, [SP+4..5]=orig dividend, [SP+6..7]=orig divisor
	; We need to push divisor then dividend (right-to-left)
	push	d		; push divisor
	push	b		; push dividend

	call	__udivmod16

	; Clean up the pushed args (4 bytes)
	pop	h		; discard
	pop	h		; discard

	; BC = quotient (unsigned)
	; Pop sign flag
	pop	psw
	ora	a		; test bit 7
	jp	.Lsd16_pos_result

	; Negate quotient in BC
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b

.Lsd16_pos_result:
	ret
	.size	__sdiv16, .-__sdiv16


; ===================================================================
; __srem16: signed 16-bit remainder
;   [SP+2..3] = dividend (s16)
;   [SP+4..5] = divisor (s16)
;   Returns: BC = remainder (sign follows dividend)
; ===================================================================
	.section .text.__srem16, "ax", @progbits
	.globl	__srem16
	.type	__srem16,@function
__srem16:
	; Load dividend into BC
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	c, l
	mov	b, h
#else
	lxi	h, 2
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
#endif

	; Load divisor into DE
#ifdef UNDOC
	ldsi	4
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 4
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Save dividend sign (remainder sign = dividend sign)
	mov	a, b
	push	psw

	; Make dividend (BC) unsigned
	mov	a, b
	ora	a
	jp	.Lsr16_a_pos
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lsr16_a_pos:

	; Make divisor (DE) unsigned
	mov	a, d
	ora	a
	jp	.Lsr16_b_pos
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lsr16_b_pos:

	; Push unsigned args for __udivmod16
	push	d		; push divisor
	push	b		; push dividend

	call	__udivmod16

	; Clean up the pushed args (4 bytes)
	pop	h		; discard
	pop	h		; discard

	; DE = remainder (unsigned), move to BC
	mov	b, d
	mov	c, e

	; Pop dividend sign
	pop	psw
	ora	a		; test bit 7
	jp	.Lsr16_pos_result

	; Negate remainder in BC
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b

.Lsr16_pos_result:
	ret
	.size	__srem16, .-__srem16


; ===================================================================
; __sdivmod16: signed 16-bit divmod
;   [SP+2..3] = dividend (s16, little-endian)
;   [SP+4..5] = divisor (s16, little-endian)
;   Returns: BC = quotient, DE = remainder
;
; Strategy: save both signs, take absolute values, call __udivmod16,
;           fix sign of quotient and remainder.
; ===================================================================
	.section .text.__sdivmod16, "ax", @progbits
	.globl	__sdivmod16
	.type	__sdivmod16,@function
__sdivmod16:
	; Load dividend into BC
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	c, l
	mov	b, h
#else
	lxi	h, 2
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
#endif

	; Load divisor into DE
#ifdef UNDOC
	ldsi	4
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 4
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Save dividend sign (for remainder sign)
	mov	a, b		; high byte of dividend
	push	psw		; [SP] = dividend sign

	; Compute quotient sign: XOR of high bytes
	mov	a, b
	xra	d
	push	psw		; [SP] = quotient sign

	; Make dividend (BC) unsigned
	mov	a, b
	ora	a
	jp	.Lsdm16_a_pos
	; Negate BC
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lsdm16_a_pos:

	; Make divisor (DE) unsigned
	mov	a, d
	ora	a
	jp	.Lsdm16_b_pos
	; Negate DE
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lsdm16_b_pos:

	; Push unsigned args for __udivmod16
	; __udivmod16 expects [SP+2..3]=dividend, [SP+4..5]=divisor
	push	d		; push divisor
	push	b		; push dividend

	call	__udivmod16

	; Clean up 4 bytes of pushed args
	pop	h		; discard
	pop	h		; discard

	; BC = unsigned quotient, DE = unsigned remainder

	; Fix quotient sign
	pop	psw		; quotient sign flag
	ora	a		; test bit 7
	jp	.Lsdm16_qpos

	; Negate quotient in BC
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lsdm16_qpos:

	; Fix remainder sign (remainder sign = dividend sign)
	pop	psw		; dividend sign flag
	ora	a		; test bit 7
	jp	.Lsdm16_rpos

	; Negate remainder in DE
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lsdm16_rpos:

	; BC = quotient, DE = remainder
	ret
	.size	__sdivmod16, .-__sdivmod16


; ===================================================================
; __udivmod32: unsigned 32-bit divmod
;   [SP+2..5]  = dividend (u32, little-endian)
;   [SP+6..9]  = divisor (u32, little-endian)
;   Returns: BC:DE = quotient (C=byte0, B=byte1, E=byte2, D=byte3)
;
; Optimized: quotient kept register-resident in BC:DE.
;   Only 8 bytes allocated on stack (dividend + remainder contiguous).
;   Dividend and remainder are shifted as one 8-byte chain (no
;   extra LXI+DAD SP between them).
;   Trial subtraction replaces MSB-first comparison (restoring
;   division: subtract, check borrow, undo if needed).
; ===================================================================
	.section .text.__udivmod32, "ax", @progbits
	.globl	__udivmod32
	.type	__udivmod32,@function
__udivmod32:
	; Check for divide by zero
	lxi	h, 6
	dad	sp
	mov	a, m
	inx	h
	ora	m
	inx	h
	ora	m
	inx	h
	ora	m
	jnz	.Ludm32_start

	; Division by zero
	lxi	b, 0
	lxi	d, 0
	ret

.Ludm32_start:
	; Copy dividend from args to stack
	lxi	h, 2
	dad	sp
	mov	a, m		; dividend[0]
	inx	h
	mov	b, m		; dividend[1]
	inx	h
	mov	e, m		; dividend[2]
	inx	h
	mov	d, m		; dividend[3]

	; Allocate 8 bytes on stack: dividend[0..3] then remainder[0..3]
	; (contiguous so we can shift all 8 bytes with one INX H chain)
	lxi	h, 0
	push	h		; remainder[2..3] = 0
	push	h		; remainder[0..1] = 0
	push	d		; dividend[2..3]
	; A still has dividend[0], B has dividend[1]
	mov	c, a		; C = dividend[0]
	push	b		; dividend[0..1] (C=byte0, B=byte1)

	; Stack layout (from current SP):
	;   [SP+ 0.. 1] = dividend[0..1]
	;   [SP+ 2.. 3] = dividend[2..3]
	;   [SP+ 4.. 5] = remainder[0..1]
	;   [SP+ 6.. 7] = remainder[2..3]
	;   [SP+ 8.. 9] = return address
	;   [SP+10..13] = arg: dividend (original)
	;   [SP+14..17] = arg: divisor

	; Initialize quotient in BC:DE to 0
	lxi	b, 0
	lxi	d, 0

	; Loop counter in A, saved/restored via PUSH/POP PSW
	mvi	a, 32

.Ludm32_loop:
	push	psw		; save counter (+2 shift)

	; --- Shift quotient left (register-resident) ---
	; C=byte0(LSB), B=byte1, E=byte2, D=byte3(MSB)
	mov	a, c
	add	a
	mov	c, a
	mov	a, b
	adc	a
	mov	b, a
	mov	a, e
	adc	a
	mov	e, a
	mov	a, d
	adc	a
	mov	d, a

	; --- Shift dividend[0..3] and remainder[0..3] as 8-byte chain ---
	; dividend at [SP+2], remainder at [SP+6] (push psw shifted +2)
	; Contiguous: dividend[0] at [SP+2] through remainder[3] at [SP+9]
	lxi	h, 2
	dad	sp		; HL -> dividend[0] (DAD clobbers carry, ok here)
	mov	a, m
	add	a		; dividend[0] <<= 1, carry = old bit 7
	mov	m, a
	inx	h		; -> dividend[1] (INX doesn't affect flags)
	mov	a, m
	adc	a
	mov	m, a
	inx	h		; -> dividend[2]
	mov	a, m
	adc	a
	mov	m, a
	inx	h		; -> dividend[3]
	mov	a, m
	adc	a
	mov	m, a		; carry = MSB of dividend
	inx	h		; -> remainder[0]
	mov	a, m
	adc	a		; remainder[0] = rem[0]*2 + carry_from_dividend
	mov	m, a
	inx	h		; -> remainder[1]
	mov	a, m
	adc	a
	mov	m, a
	inx	h		; -> remainder[2]
	mov	a, m
	adc	a
	mov	m, a
	inx	h		; -> remainder[3]
	mov	a, m
	adc	a
	mov	m, a

	; --- Trial subtraction (restoring division) ---
	; Save quotient BC:DE to free registers for subtract
	push	d		; quotient high (+2)
	push	b		; quotient low (+2)
	; SP shifted +6 total (push psw + push d + push b)

	; Stack layout now:
	;   [SP+ 0.. 1] = saved BC (quotient low)
	;   [SP+ 2.. 3] = saved DE (quotient high)
	;   [SP+ 4.. 5] = saved PSW (counter)
	;   [SP+ 6.. 7] = dividend[0..1]
	;   [SP+ 8.. 9] = dividend[2..3]
	;   [SP+10..11] = remainder[0..1]
	;   [SP+12..13] = remainder[2..3]
	;   [SP+14..15] = return address
	;   [SP+16..19] = arg: dividend
	;   [SP+20..23] = arg: divisor

	; Load divisor into C,B,E,D
	lxi	h, 20
	dad	sp
	mov	c, m		; divisor[0]
	inx	h
	mov	b, m		; divisor[1]
	inx	h
	mov	e, m		; divisor[2]
	inx	h
	mov	d, m		; divisor[3]

	; Trial subtract: remainder -= divisor
	lxi	h, 10
	dad	sp		; HL -> remainder[0]
	mov	a, m
	sub	c
	mov	m, a		; remainder[0] -= divisor[0]
	inx	h
	mov	a, m
	sbb	b
	mov	m, a		; remainder[1]
	inx	h
	mov	a, m
	sbb	e
	mov	m, a		; remainder[2]
	inx	h
	mov	a, m
	sbb	d
	mov	m, a		; remainder[3]

	; If carry set: remainder was < divisor, undo subtraction
	jc	.Ludm32_restore

	; remainder >= divisor: keep subtraction, set quotient bit 0
	pop	b		; restore quotient low
	pop	d		; restore quotient high
	inr	c		; set bit 0 (C was just shifted left, so bit 0 = 0)
	jmp	.Ludm32_next

.Ludm32_restore:
	; Undo: remainder += divisor (C,B,E,D still hold divisor)
	lxi	h, 10
	dad	sp		; HL -> remainder[0]
	mov	a, m
	add	c
	mov	m, a
	inx	h
	mov	a, m
	adc	b
	mov	m, a
	inx	h
	mov	a, m
	adc	e
	mov	m, a
	inx	h
	mov	a, m
	adc	d
	mov	m, a

	; Restore quotient (bit 0 stays 0)
	pop	b
	pop	d

.Ludm32_next:
	; Restore counter and loop
	pop	psw
	dcr	a
	jnz	.Ludm32_loop

	; BC:DE already has quotient
	; Deallocate 8 bytes (dividend + remainder)
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__udivmod32, .-__udivmod32


; ===================================================================
; __udiv32: unsigned 32-bit divide
;   [SP+2..5]  = dividend (u32)
;   [SP+6..9]  = divisor (u32)
;   Returns: BC:DE = quotient
;
; Tail-call: JMP so __udivmod32 sees the same stack frame.
; ===================================================================
	.section .text.__udiv32, "ax", @progbits
	.globl	__udiv32
	.type	__udiv32,@function
__udiv32:
	jmp	__udivmod32
	; __udivmod32 returns directly to our caller with BC:DE=quotient
	.size	__udiv32, .-__udiv32


; ===================================================================
; __urem32: unsigned 32-bit remainder
;   [SP+2..5]  = dividend (u32)
;   [SP+6..9]  = divisor (u32)
;   Returns: BC:DE = remainder
;
; Optimized: no quotient needed, so BC:DE are free during the loop.
;   Uses contiguous dividend+remainder layout and trial subtraction
;   (restoring division).
; ===================================================================
	.section .text.__urem32, "ax", @progbits
	.globl	__urem32
	.type	__urem32,@function
__urem32:
	; Check for divide by zero
	lxi	h, 6
	dad	sp
	mov	a, m
	inx	h
	ora	m
	inx	h
	ora	m
	inx	h
	ora	m
	jnz	.Lurm32_start

	; Division by zero
	lxi	b, 0
	lxi	d, 0
	ret

.Lurm32_start:
	; Copy dividend to stack, allocate remainder
	lxi	h, 2
	dad	sp
	mov	a, m		; dividend[0]
	inx	h
	mov	b, m		; dividend[1]
	inx	h
	mov	e, m		; dividend[2]
	inx	h
	mov	d, m		; dividend[3]

	; Allocate 8 bytes: dividend[0..3] then remainder[0..3] (contiguous)
	lxi	h, 0
	push	h		; remainder[2..3] = 0
	push	h		; remainder[0..1] = 0
	push	d		; dividend[2..3]
	mov	c, a		; C = dividend[0]
	push	b		; dividend[0..1]

	; Stack layout (from current SP):
	;   [SP+ 0.. 1] = dividend[0..1]
	;   [SP+ 2.. 3] = dividend[2..3]
	;   [SP+ 4.. 5] = remainder[0..1]
	;   [SP+ 6.. 7] = remainder[2..3]
	;   [SP+ 8.. 9] = return address
	;   [SP+10..13] = arg: dividend
	;   [SP+14..17] = arg: divisor

	mvi	a, 32		; loop counter

.Lurm32_loop:
	push	psw		; save counter (+2)

	; --- Shift dividend[0..3] and remainder[0..3] as 8-byte chain ---
	; dividend at [SP+2], remainder at [SP+6] (push psw shifted +2)
	lxi	h, 2
	dad	sp		; HL -> dividend[0]
	mov	a, m
	add	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; carry = MSB of dividend
	inx	h		; -> remainder[0]
	mov	a, m
	adc	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a

	; --- Trial subtraction: remainder -= divisor ---
	; No quotient to save -- BC:DE are free
	; Load divisor into C,B,E,D
	lxi	h, 16
	dad	sp		; divisor at [SP+16] (push psw +2)
	mov	c, m		; divisor[0]
	inx	h
	mov	b, m		; divisor[1]
	inx	h
	mov	e, m		; divisor[2]
	inx	h
	mov	d, m		; divisor[3]

	; Trial subtract: remainder -= divisor
	lxi	h, 6
	dad	sp		; HL -> remainder[0]
	mov	a, m
	sub	c
	mov	m, a
	inx	h
	mov	a, m
	sbb	b
	mov	m, a
	inx	h
	mov	a, m
	sbb	e
	mov	m, a
	inx	h
	mov	a, m
	sbb	d
	mov	m, a

	; If carry: remainder was < divisor, restore
	jnc	.Lurm32_no_restore

	; Undo: remainder += divisor
	lxi	h, 6
	dad	sp		; HL -> remainder[0]
	mov	a, m
	add	c
	mov	m, a
	inx	h
	mov	a, m
	adc	b
	mov	m, a
	inx	h
	mov	a, m
	adc	e
	mov	m, a
	inx	h
	mov	a, m
	adc	d
	mov	m, a

.Lurm32_no_restore:
	pop	psw
	dcr	a
	jnz	.Lurm32_loop

	; Load remainder into BC:DE
	lxi	h, 4
	dad	sp
	mov	c, m		; remainder[0] -> C
	inx	h
	mov	b, m		; remainder[1] -> B
	inx	h
	mov	e, m		; remainder[2] -> E
	inx	h
	mov	d, m		; remainder[3] -> D

	; Deallocate 8 bytes
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__urem32, .-__urem32


; ===================================================================
; __sdiv32: signed 32-bit divide
;   [SP+2..5]  = dividend (s32)
;   [SP+6..9]  = divisor (s32)
;   Returns: BC:DE = quotient
; ===================================================================
	.section .text.__sdiv32, "ax", @progbits
	.globl	__sdiv32
	.type	__sdiv32,@function
__sdiv32:
	; Compute result sign: XOR of sign bytes
#ifdef UNDOC
	ldsi	5
	ldax	d		; dividend[3]
#else
	lxi	h, 5
	dad	sp
	mov	a, m		; dividend[3]
#endif
	lxi	h, 9
	dad	sp
	xra	m		; XOR with divisor[3]
	push	psw		; save sign flag

	; Make dividend unsigned (negate if negative)
	; dividend at [SP+4..7] (sign push +2)
#ifdef UNDOC
	ldsi	7
	ldax	d		; dividend[3]
#else
	lxi	h, 7
	dad	sp
	mov	a, m		; dividend[3]
#endif
	ora	a
	jp	.Lsd32_a_pos

	; Negate dividend in-place on caller's stack
	lxi	h, 4
	dad	sp
	mov	a, m
	cma
	adi	1
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
.Lsd32_a_pos:

	; Make divisor unsigned
#ifdef UNDOC
	ldsi	11
	ldax	d		; divisor[3]
#else
	lxi	h, 11
	dad	sp
	mov	a, m		; divisor[3]
#endif
	ora	a
	jp	.Lsd32_b_pos

	lxi	h, 8
	dad	sp
	mov	a, m
	cma
	adi	1
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
.Lsd32_b_pos:

	; Push unsigned divisor and dividend for __udivmod32
	; __udivmod32 expects [SP+2..5]=dividend, [SP+6..9]=divisor
	; Current: [SP+0..1]=sign, [SP+2..3]=ret addr, [SP+4..7]=dividend, [SP+8..11]=divisor

	; Load divisor
	lxi	h, 8
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d		; divisor[2..3]
	push	b		; divisor[0..1]

	; Load dividend
	; After push psw (+2), push d (+2), push b (+2) = +6 total
	; dividend was at [SP_orig+2..5], now at [SP+8..11]
	lxi	h, 8
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d		; dividend[2..3]
	push	b		; dividend[0..1]

	call	__udivmod32

	; Clean up 8 bytes of pushed args
	lxi	h, 8
	dad	sp
	sphl

	; BC:DE = quotient (unsigned)
	; Pop sign flag
	pop	psw
	ora	a		; test bit 7
	jp	.Lsd32_pos_result

	; Negate 32-bit quotient in BC:DE
	; C=byte0, B=byte1, E=byte2, D=byte3
	mov	a, c
	cma
	adi	1
	mov	c, a
	mov	a, b
	cma
	aci	0
	mov	b, a
	mov	a, e
	cma
	aci	0
	mov	e, a
	mov	a, d
	cma
	aci	0
	mov	d, a

.Lsd32_pos_result:
	ret
	.size	__sdiv32, .-__sdiv32


; ===================================================================
; __srem32: signed 32-bit remainder
;   [SP+2..5]  = dividend (s32)
;   [SP+6..9]  = divisor (s32)
;   Returns: BC:DE = remainder (sign follows dividend)
; ===================================================================
	.section .text.__srem32, "ax", @progbits
	.globl	__srem32
	.type	__srem32,@function
__srem32:
	; Save dividend sign (remainder sign = dividend sign)
#ifdef UNDOC
	ldsi	5
	ldax	d		; dividend[3] (sign byte)
#else
	lxi	h, 5
	dad	sp
	mov	a, m		; dividend[3] (sign byte)
#endif
	push	psw		; save sign flag

	; Make dividend unsigned
#ifdef UNDOC
	ldsi	7
	ldax	d
#else
	lxi	h, 7
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lsr32_a_pos

	lxi	h, 4
	dad	sp
	mov	a, m
	cma
	adi	1
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
.Lsr32_a_pos:

	; Make divisor unsigned
#ifdef UNDOC
	ldsi	11
	ldax	d
#else
	lxi	h, 11
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lsr32_b_pos

	lxi	h, 8
	dad	sp
	mov	a, m
	cma
	adi	1
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a
.Lsr32_b_pos:

	; Push divisor then dividend for __urem32's call to divmod core
	; We'll call __urem32 which handles the remainder extraction.
	; Actually, let's just inline the same pattern as __sdiv32 but
	; call the remainder path.

	; Load divisor
	lxi	h, 8
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d
	push	b

	; Load dividend
	; After push psw (+2), push d (+2), push b (+2) = +6 total
	; dividend was at [SP_orig+2..5], now at [SP+8..11]
	lxi	h, 8
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d
	push	b

	call	__urem32

	; Clean up 8 bytes
	lxi	h, 8
	dad	sp
	sphl

	; BC:DE = remainder (unsigned)
	; Pop dividend sign
	pop	psw
	ora	a		; test bit 7
	jp	.Lsr32_pos_result

	; Negate remainder
	mov	a, c
	cma
	adi	1
	mov	c, a
	mov	a, b
	cma
	aci	0
	mov	b, a
	mov	a, e
	cma
	aci	0
	mov	e, a
	mov	a, d
	cma
	aci	0
	mov	d, a

.Lsr32_pos_result:
	ret
	.size	__srem32, .-__srem32
