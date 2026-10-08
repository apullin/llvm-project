; Hand-optimised i8085 64-bit division/remainder routines.
;
; Replaces the C-compiled int_divdi3.c which uses a 64-iteration
; restoring division loop with i64 operations, producing enormous
; code when compiled through the i8085 backend.
;
; Calling convention (sret for i64 return):
;   __udivmoddi4: void __udivmoddi4(uint64_t *quotient, uint64_t a,
;                                    uint64_t b, uint64_t *rem)
;     [SP+2..3]   = sret pointer (quotient output)
;     [SP+4..11]  = a (dividend, 8 bytes, little-endian)
;     [SP+12..19] = b (divisor, 8 bytes, little-endian)
;     [SP+20..21] = rem pointer (remainder output, may be NULL)
;
;   __udivdi3, __umoddi3: standard sret convention
;     [SP+2..3]   = sret pointer
;     [SP+4..11]  = a (8 bytes)
;     [SP+12..19] = b (8 bytes)
;
;   __divdi3, __moddi3: standard sret convention (signed)
;     [SP+2..3]   = sret pointer
;     [SP+4..11]  = a (8 bytes)
;     [SP+12..19] = b (8 bytes)
;
; Algorithm: 64-iteration restoring division (MSB-first).
;   quotient = 0
;   remainder = 0
;   for each bit from MSB to LSB (64 iterations):
;     remainder = (remainder << 1) | MSB(dividend)
;     dividend <<= 1
;     if remainder >= divisor:
;       remainder -= divisor
;       quotient = (quotient << 1) | 1
;     else:
;       quotient <<= 1


; ===================================================================
; __udivmoddi4: unsigned 64-bit divmod (core routine)
;
; All working data on stack. 64 iterations.
;
; Stack frame:
;   quotient[0..7]   (8 bytes, initialized to 0)
;   remainder[0..7]  (8 bytes, initialized to 0)
;   dividend[0..7]   (8 bytes, copy of a)
;   counter          (1 byte, counts down from 64)
;
; Total local allocation: 25 bytes (padded to 26 for alignment)
; ===================================================================
	.section .text.__udivmoddi4, "ax", @progbits
	.globl	__udivmoddi4
	.type	__udivmoddi4,@function
__udivmoddi4:
	; Check for divide by zero
	lxi	h, 12
	dad	sp
	mov	a, m		; b[0]
	inx	h
	ora	m		; | b[1]
	inx	h
	ora	m		; | b[2]
	inx	h
	ora	m		; | b[3]
	inx	h
	ora	m		; | b[4]
	inx	h
	ora	m		; | b[5]
	inx	h
	ora	m		; | b[6]
	inx	h
	ora	m		; | b[7]
	jnz	.Ludmd64_start

	; Division by zero: write 0 to sret, 0 to rem if non-null
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h		; DE = sret
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret
#endif
	xra	a
	stax	d
	inx	d
	stax	d
	inx	d
	stax	d
	inx	d
	stax	d
	inx	d
	stax	d
	inx	d
	stax	d
	inx	d
	stax	d
	inx	d
	stax	d
	ret

.Ludmd64_start:
	; Copy dividend (a) from args to stack
	lxi	h, 4
	dad	sp
	mov	c, m		; a[0]
	inx	h
	mov	b, m		; a[1]
	inx	h
	mov	e, m		; a[2]
	inx	h
	mov	d, m		; a[3]
	push	d		; dividend[2..3]
	push	b		; dividend[0..1]

	; a[4..7]
	lxi	h, 12		; a[4] at original SP+8, shifted +4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m

	; We need to push dividend[4..7] BELOW dividend[0..3] on stack.
	; But we already pushed dividend[0..3]. Let me rethink.
	; We want stack layout: [low addr] quotient, remainder, dividend [high addr]
	; Let's allocate everything then fill it in.

	; Undo the pushes
	pop	b
	pop	d

	; Allocate 24 bytes for quotient(8) + remainder(8) + dividend(8)
	; plus 2 more for counter (using a push)
	; Initialize quotient and remainder to 0.
	lxi	h, 0
	push	h		; dividend[6..7] placeholder
	push	h		; dividend[4..5] placeholder
	push	h		; dividend[2..3] placeholder
	push	h		; dividend[0..1] placeholder
	push	h		; remainder[6..7] = 0
	push	h		; remainder[4..5] = 0
	push	h		; remainder[2..3] = 0
	push	h		; remainder[0..1] = 0
	push	h		; quotient[6..7] = 0
	push	h		; quotient[4..5] = 0
	push	h		; quotient[2..3] = 0
	push	h		; quotient[0..1] = 0

	; Stack layout (from current SP):
	;   [SP+ 0.. 1] = quotient[0..1]
	;   [SP+ 2.. 3] = quotient[2..3]
	;   [SP+ 4.. 5] = quotient[4..5]
	;   [SP+ 6.. 7] = quotient[6..7]
	;   [SP+ 8.. 9] = remainder[0..1]
	;   [SP+10..11] = remainder[2..3]
	;   [SP+12..13] = remainder[4..5]
	;   [SP+14..15] = remainder[6..7]
	;   [SP+16..17] = dividend[0..1]
	;   [SP+18..19] = dividend[2..3]
	;   [SP+20..21] = dividend[4..5]
	;   [SP+22..23] = dividend[6..7]
	;   [SP+24..25] = return address
	;   [SP+26..27] = sret pointer
	;   [SP+28..35] = a (original arg)
	;   [SP+36..43] = b (divisor)
	;   [SP+44..45] = rem pointer

	; Copy dividend from args to local stack
	lxi	h, 28
	dad	sp		; -> a[0]
	mov	c, m
	inx	h
	mov	b, m
	lxi	h, 16
	dad	sp
	mov	m, c		; dividend[0]
	inx	h
	mov	m, b		; dividend[1]

	lxi	h, 30
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	lxi	h, 18
	dad	sp
	mov	m, c		; dividend[2]
	inx	h
	mov	m, b		; dividend[3]

	lxi	h, 32
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	lxi	h, 20
	dad	sp
	mov	m, c		; dividend[4]
	inx	h
	mov	m, b		; dividend[5]

	lxi	h, 34
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	lxi	h, 22
	dad	sp
	mov	m, c		; dividend[6]
	inx	h
	mov	m, b		; dividend[7]

	; Loop counter: 64 iterations
	mvi	e, 64

.Ludmd64_loop:
	push	d		; save counter (+2 shift)

	; --- Shift dividend left by 1 bit ---
	; dividend at [SP+18] (shifted +2 for push)
	lxi	h, 18
	dad	sp
	mov	a, m
	add	a
	mov	m, a		; dividend[0] <<= 1
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[1]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[2]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[3]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[4]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[5]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[6]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; dividend[7], carry = old MSB

	; --- Shift remainder left, bring in carry from dividend ---
	; remainder at [SP+10] (shifted +2)
	; DAD SP will destroy carry, so save it first.
	push	psw		; save carry (+2 more, total +4)
	lxi	h, 12
	dad	sp		; remainder[0] at SP+8+4=12
	pop	psw		; restore carry

	mov	a, m
	adc	a		; remainder[0] = remainder[0]*2 + carry_from_dividend
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[1]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[2]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[3]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[4]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[5]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[6]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; remainder[7]

	; --- Shift quotient left by 1 ---
	; quotient at [SP+2] (shifted +2)
	lxi	h, 2
	dad	sp
	mov	a, m
	add	a
	mov	m, a		; quotient[0] <<= 1
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[1]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[2]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[3]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[4]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[5]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[6]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; quotient[7]

	; --- Compare remainder >= divisor ---
	; Compare from MSB (byte 7) to LSB (byte 0)
	; remainder at [SP+10..17] (shifted +2)
	; divisor at [SP+38..45] (shifted +2)

	; Byte 7
	lxi	h, 17
	dad	sp
	mov	b, m		; remainder[7]
	lxi	h, 45
	dad	sp
	mov	a, b
	cmp	m		; remainder[7] vs divisor[7]
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 6
	lxi	h, 16
	dad	sp
	mov	b, m
	lxi	h, 44
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 5
	lxi	h, 15
	dad	sp
	mov	b, m
	lxi	h, 43
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 4
	lxi	h, 14
	dad	sp
	mov	b, m
	lxi	h, 42
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 3
	lxi	h, 13
	dad	sp
	mov	b, m
	lxi	h, 41
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 2
	lxi	h, 12
	dad	sp
	mov	b, m
	lxi	h, 40
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 1
	lxi	h, 11
	dad	sp
	mov	b, m
	lxi	h, 39
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub
	jnz	.Ludmd64_do_sub

	; Byte 0
	lxi	h, 10
	dad	sp
	mov	b, m
	lxi	h, 38
	dad	sp
	mov	a, b
	cmp	m
	jc	.Ludmd64_no_sub

.Ludmd64_do_sub:
	; remainder -= divisor (8-byte subtract)
	; Load divisor[0..3] into registers
	lxi	h, 38
	dad	sp
	mov	c, m		; divisor[0]
	inx	h
	mov	b, m		; divisor[1]
	inx	h
	mov	e, m		; divisor[2]
	inx	h
	mov	d, m		; divisor[3]

	; Subtract from remainder[0..3]
	lxi	h, 10
	dad	sp
	mov	a, m
	sub	c
	mov	m, a		; remainder[0]
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

	; Save borrow for bytes 4-7
	; DAD SP will destroy carry, so save it
	push	psw		; save borrow (+2)

	; Load divisor[4..7]
	lxi	h, 44		; divisor[4] at 42+2
	dad	sp
	mov	c, m		; divisor[4]
	inx	h
	mov	b, m		; divisor[5]
	inx	h
	mov	e, m		; divisor[6]
	inx	h
	mov	d, m		; divisor[7]

	lxi	h, 16		; remainder[4] at 14+2
	dad	sp

	pop	psw		; restore borrow

	mov	a, m
	sbb	c
	mov	m, a		; remainder[4]
	inx	h
	mov	a, m
	sbb	b
	mov	m, a		; remainder[5]
	inx	h
	mov	a, m
	sbb	e
	mov	m, a		; remainder[6]
	inx	h
	mov	a, m
	sbb	d
	mov	m, a		; remainder[7]

	; Set bit 0 of quotient
	lxi	h, 2
	dad	sp
	mov	a, m
	ori	1
	mov	m, a

.Ludmd64_no_sub:
	; Restore counter
	pop	d		; E = counter (SP back to normal)
	dcr	e
	jnz	.Ludmd64_loop

	; --- Done: write quotient to sret pointer ---
	lxi	h, 26
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer

	lxi	h, 0
	dad	sp
	; Write quotient[0..3]
	mov	a, m
	stax	d		; [sret+0]
	inx	h
	inx	d
	mov	a, m
	stax	d
	inx	h
	inx	d
	mov	a, m
	stax	d
	inx	h
	inx	d
	mov	a, m
	stax	d
	inx	h
	inx	d

	; Write quotient[4..7] using HL for sret pointer
	; DE = sret+4, HL = quotient[4]
	push	d		; save sret+4 (+2)
	mov	c, m		; quotient[4]
	inx	h
	mov	b, m		; quotient[5]
	inx	h
	mov	e, m		; quotient[6]
	inx	h
	mov	d, m		; quotient[7]
	pop	h		; HL = sret+4
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	; --- Write remainder to rem pointer (if non-null) ---
	; rem pointer at [SP+44..45]
	lxi	h, 44
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = rem pointer

	; Check if NULL
	mov	a, d
	ora	e
	jz	.Ludmd64_cleanup

	; Write remainder[0..3]
	lxi	h, 8
	dad	sp
	mov	a, m
	stax	d		; [rem+0]
	inx	h
	inx	d
	mov	a, m
	stax	d
	inx	h
	inx	d
	mov	a, m
	stax	d
	inx	h
	inx	d
	mov	a, m
	stax	d
	inx	h
	inx	d

	; Write remainder[4..7]
	push	d		; save rem+4 (+2)
	mov	c, m		; remainder[4]
	inx	h
	mov	b, m		; remainder[5]
	inx	h
	mov	e, m		; remainder[6]
	inx	h
	mov	d, m		; remainder[7]
	pop	h		; HL = rem+4
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

.Ludmd64_cleanup:
	; Deallocate 24 bytes of stack locals
	lxi	h, 24
	dad	sp
	sphl

	ret
	.size	__udivmoddi4, .-__udivmoddi4


; ===================================================================
; __udivdi3: unsigned 64-bit divide
;   void __udivdi3(uint64_t *result, uint64_t a, uint64_t b)
;   [SP+2..3]   = sret pointer
;   [SP+4..11]  = a (8 bytes)
;   [SP+12..19] = b (8 bytes)
;
; Wrapper: calls __udivmoddi4 with NULL rem pointer.
; ===================================================================
	.section .text.__udivdi3, "ax", @progbits
	.globl	__udivdi3
	.type	__udivdi3,@function
__udivdi3:
	; Push NULL rem pointer
	lxi	h, 0
	push	h		; rem = NULL (+2)

	; Push divisor (b) — high pair first, low pair last.
	; After push NULL (+2): b at [SP+14..21], b[7] at SP+21, b[6] at SP+20.
	; Each push shifts by +2, but the target pair also moves +2,
	; so the high byte of each pair stays at SP+21.
	lxi	h, 21
	dad	sp
	mov	d, m		; b[7]
	dcx	h
	mov	e, m		; b[6]
	push	d		; b[6..7]
	lxi	h, 21
	dad	sp
	mov	d, m		; b[5]
	dcx	h
	mov	e, m		; b[4]
	push	d		; b[4..5]
	lxi	h, 21
	dad	sp
	mov	d, m		; b[3]
	dcx	h
	mov	e, m		; b[2]
	push	d		; b[2..3]
	lxi	h, 21
	dad	sp
	mov	d, m		; b[1]
	dcx	h
	mov	e, m		; b[0]
	push	d		; b[0..1] — total pushed: 10 bytes

	; Push dividend (a) — same sliding-window pattern.
	; After 10 bytes pushed: a at [SP+14..21], a[7] at SP+21.
	lxi	h, 21
	dad	sp
	mov	d, m		; a[7]
	dcx	h
	mov	e, m		; a[6]
	push	d		; a[6..7]
	lxi	h, 21
	dad	sp
	mov	d, m		; a[5]
	dcx	h
	mov	e, m		; a[4]
	push	d		; a[4..5]
	lxi	h, 21
	dad	sp
	mov	d, m		; a[3]
	dcx	h
	mov	e, m		; a[2]
	push	d		; a[2..3]
	lxi	h, 21
	dad	sp
	mov	d, m		; a[1]
	dcx	h
	mov	e, m		; a[0]
	push	d		; a[0..1] — total pushed: 18 bytes

	; Push sret pointer from [SP+2+18=20..3+18=21]
	lxi	h, 20
	dad	sp
	mov	e, m		; sret_lo
	inx	h
	mov	d, m		; sret_hi
	push	d		; total pushed: 20 bytes

	call	__udivmoddi4

	; Clean up: 20 bytes pushed
	lxi	h, 20
	dad	sp
	sphl

	ret
	.size	__udivdi3, .-__udivdi3


; ===================================================================
; __umoddi3: unsigned 64-bit modulo
;   void __umoddi3(uint64_t *result, uint64_t a, uint64_t b)
;   [SP+2..3]   = sret pointer (where to write remainder)
;   [SP+4..11]  = a (8 bytes)
;   [SP+12..19] = b (8 bytes)
;
; Calls __udivmoddi4 with sret as rem pointer, and a temp as quotient.
; ===================================================================
	.section .text.__umoddi3, "ax", @progbits
	.globl	__umoddi3
	.type	__umoddi3,@function
__umoddi3:
	; We need a temp area for the quotient (8 bytes) on stack.
	; Then call __udivmoddi4 with:
	;   sret = &temp (quotient, discarded)
	;   a, b from our args
	;   rem = our sret pointer (where we want the remainder)

	; Allocate 8-byte temp for quotient
	lxi	h, 0
	push	h		; temp[6..7]
	push	h		; temp[4..5]
	push	h		; temp[2..3]
	push	h		; temp[0..1]

	; Stack shifted +8:
	;   [SP+0..7]   = temp (quotient area)
	;   [SP+8..9]   = return address
	;   [SP+10..11] = sret (our sret = rem destination)
	;   [SP+12..19] = a
	;   [SP+20..27] = b

	; Push rem pointer = our sret pointer
#ifdef UNDOC
	ldsi	10
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 10
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif
	push	d		; rem pointer (+2)

	; Push divisor (b) from [SP+20+10=30... wait, shifted by temp(8)+push(2)=10]
	; b at original SP+12+8=20... after push rem (+2) = 22
	lxi	h, 28		; b[6..7] at SP+20+8+2=30... recalculate
	; Original args: sret at SP+2, a at SP+4, b at SP+12
	; After temp alloc (+8): sret at SP+10, a at SP+12, b at SP+20
	; After push rem (+2): sret at SP+12, a at SP+14, b at SP+22
	; b[7] at SP+29, b[6] at SP+28
	lxi	h, 29
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; b[6..7] (+2)
	lxi	h, 29		; b[5] at SP+22+3+4=29... no
	; After 2 more bytes pushed: b at SP+22+2=24
	; b[4] at SP+28, b[5] at SP+29
	dad	sp
	mov	d, m		; b[5]
	dcx	h
	mov	e, m		; b[4]
	push	d
	lxi	h, 29
	dad	sp
	mov	d, m		; b[3]
	dcx	h
	mov	e, m		; b[2]
	push	d
	lxi	h, 29
	dad	sp
	mov	d, m		; b[1]
	dcx	h
	mov	e, m		; b[0]
	push	d		; pushed rem(2) + b(8) = 10

	; Push dividend (a): at original SP+4, now SP+4+8+10=22
	; a[7] at SP+29, a[6] at SP+28... No, let me carefully recalculate.
	; Original: [SP+2]=sret, [SP+4..11]=a, [SP+12..19]=b
	; Temp alloc (+8): a at SP+12..19, b at SP+20..27
	; Push rem (+2): a at SP+14..21
	; Push b[6..7] (+2): a at SP+16..23
	; Push b[4..5] (+2): a at SP+18..25
	; Push b[2..3] (+2): a at SP+20..27
	; Push b[0..1] (+2): a at SP+22..29
	; So a[6..7] at SP+28..29
	lxi	h, 29
	dad	sp
	mov	d, m		; a[7]
	dcx	h
	mov	e, m		; a[6]
	push	d
	lxi	h, 29
	dad	sp
	mov	d, m		; a[5]
	dcx	h
	mov	e, m		; a[4]
	push	d
	lxi	h, 29
	dad	sp
	mov	d, m		; a[3]
	dcx	h
	mov	e, m		; a[2]
	push	d
	lxi	h, 29
	dad	sp
	mov	d, m		; a[1]
	dcx	h
	mov	e, m		; a[0]
	push	d		; pushed total: rem(2) + b(8) + a(8) = 18

	; Push sret = address of temp area on stack
	; Temp area was at original_SP - 8 (before we started pushing args).
	; After pushing 18 bytes of args, temp is at SP + 18.
	; We need the address of SP+18.
	lxi	h, 18
	dad	sp
	push	h		; sret for quotient (address of temp)

	; Total pushed: 20 bytes for __udivmoddi4 args

	call	__udivmoddi4

	; Clean up: 20 bytes of args
	lxi	h, 20
	dad	sp
	sphl

	; Deallocate 8-byte temp
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__umoddi3, .-__umoddi3


; ===================================================================
; __divdi3: signed 64-bit divide
;   void __divdi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]   = sret pointer
;   [SP+4..11]  = a (8 bytes)
;   [SP+12..19] = b (8 bytes)
;
; Strategy: determine sign of result, make both operands positive,
; call __udivdi3, negate result if needed.
; ===================================================================
	.section .text.__divdi3, "ax", @progbits
	.globl	__divdi3
	.type	__divdi3,@function
__divdi3:
	; Compute result sign: XOR of sign bytes
#ifdef UNDOC
	ldsi	11
	ldax	d		; a[7] (sign of a)
#else
	lxi	h, 11
	dad	sp
	mov	a, m		; a[7] (sign of a)
#endif
	lxi	h, 19
	dad	sp
	xra	m		; XOR with b[7]
	push	psw		; save sign flag (+2)

	; Make a unsigned (negate if negative)
	; a[7] at SP+4+7+2=13
#ifdef UNDOC
	ldsi	13
	ldax	d
#else
	lxi	h, 13
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Ldivdi3_a_pos

	; Negate a in-place on caller's stack
	lxi	h, 6		; a[0] at SP+4+2=6
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
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a

.Ldivdi3_a_pos:
	; Make b unsigned
#ifdef UNDOC
	ldsi	21
	ldax	d
#else
	lxi	h, 21		; b[7] at SP+12+7+2=21
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Ldivdi3_b_pos

	lxi	h, 14		; b[0] at SP+12+2=14
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
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a

.Ldivdi3_b_pos:
	; Call __udivdi3 with the now-unsigned values.
	; We can just tail-call since args are in the right positions
	; (sret, |a|, |b| on the caller's stack, which we modified in-place).
	; But we need to negate the result after, so we can't tail-call.

	; Push b (8 bytes) — sliding window, high byte at SP+21
	; After push psw (+2): b at [SP+14..21], b[7] at SP+21.
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; b[6..7]
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; b[4..5]
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; b[2..3]
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; b[0..1]

	; Push a (8 bytes) — same sliding window
	; After 8 bytes for b: a at [SP+14..21], a[7] at SP+21.
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; a[6..7]
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; a[4..5]
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; a[2..3]
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d		; a[0..1]

	; Push sret pointer from [SP+2+18=20..SP+3+18=21]
	lxi	h, 20
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
	push	d		; sret

	; Total pushed: 20 bytes (push psw + 8 b + 8 a + 2 sret)
	call	__udivdi3

	; Clean up 18 bytes
	lxi	h, 18
	dad	sp
	sphl

	; Pop sign flag
	pop	psw
	ora	a		; test bit 7
	jp	.Ldivdi3_done

	; Negate the result at sret pointer
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer

	; Read result, negate, write back
	; Read bytes 0-3
	xchg			; HL = sret
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
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a

.Ldivdi3_done:
	ret
	.size	__divdi3, .-__divdi3


; ===================================================================
; __moddi3: signed 64-bit modulo
;   void __moddi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]   = sret pointer
;   [SP+4..11]  = a (8 bytes)
;   [SP+12..19] = b (8 bytes)
;
; Strategy: remainder sign follows dividend sign.
; Make both operands positive, call __umoddi3, negate result
; if dividend was negative.
; ===================================================================
	.section .text.__moddi3, "ax", @progbits
	.globl	__moddi3
	.type	__moddi3,@function
__moddi3:
	; Save dividend sign (remainder sign = dividend sign)
#ifdef UNDOC
	ldsi	11
	ldax	d		; a[7]
#else
	lxi	h, 11
	dad	sp
	mov	a, m		; a[7]
#endif
	push	psw		; save sign (+2)

	; Make a unsigned
#ifdef UNDOC
	ldsi	13
	ldax	d
#else
	lxi	h, 13
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lmoddi3_a_pos

	lxi	h, 6
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
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a

.Lmoddi3_a_pos:
	; Make b unsigned
#ifdef UNDOC
	ldsi	21
	ldax	d
#else
	lxi	h, 21
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lmoddi3_b_pos

	lxi	h, 14
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
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a

.Lmoddi3_b_pos:
	; Push b — sliding window, high byte at SP+21
	; After push psw (+2): b at [SP+14..21], b[7] at SP+21.
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d

	; Push a — same sliding window
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d
	lxi	h, 21
	dad	sp
	mov	d, m
	dcx	h
	mov	e, m
	push	d

	; Push sret pointer from [SP+2+18=20..SP+3+18=21]
	lxi	h, 20
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
	push	d

	call	__umoddi3

	; Clean up 18 bytes
	lxi	h, 18
	dad	sp
	sphl

	; Pop sign
	pop	psw
	ora	a
	jp	.Lmoddi3_done

	; Negate result at sret
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
	xchg
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
	inx	h
	mov	a, m
	cma
	aci	0
	mov	m, a

.Lmoddi3_done:
	ret
	.size	__moddi3, .-__moddi3
