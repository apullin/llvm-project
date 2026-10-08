; Hand-optimised i8085 32-bit shift routines.
;
; Calling convention:
;   [SP+2..5] = value (i32, little-endian: byte0 at SP+2)
;   [SP+6..9] = shift_amount (i32, only low byte at SP+6 used)
;   Return: BC:DE (C=byte0/LSB, B=byte1, E=byte2, D=byte3/MSB)
;
; Algorithm: byte-shuffle for multiples of 8, then bit-level
;            loop for the remaining 0-7 bits.

; ===================================================================
; __ashlsi3: i32 left shift
; ===================================================================
	.section .text.__ashlsi3, "ax", @progbits
	.globl	__ashlsi3
	.type	__ashlsi3,@function
__ashlsi3:
#ifdef UNDOC
	ldsi	6
	ldax	d		; A = shift amount
#else
	lxi	h, 6
	dad	sp
	mov	a, m		; A = shift amount
#endif
	ani	0x1F		; mask to 0-31 (sets Z if 0)
	; Load value; lxi/dad/mov/inx preserve A and Z flag
	lxi	h, 2
	dad	sp
	mov	c, m		; byte0 (LSB)
	inx	h
	mov	b, m		; byte1
	inx	h
	mov	e, m		; byte2
	inx	h
	mov	d, m		; byte3 (MSB)
	rz			; shift by 0

	cpi	24
	jnc	.Lashl_24
	cpi	16
	jnc	.Lashl_16
	cpi	8
	jnc	.Lashl_8
	jmp	.Lashl_bits

.Lashl_24:
	mov	d, c
	mvi	e, 0
	mvi	b, 0
	mvi	c, 0
	sui	24		; A = remaining (mvi doesn't touch A)
	jz	.Lashl_done
	jmp	.Lashl_bits

.Lashl_16:
	mov	d, b
	mov	e, c
	mvi	b, 0
	mvi	c, 0
	sui	16
	jz	.Lashl_done
	jmp	.Lashl_bits

.Lashl_8:
	mov	d, e
	mov	e, b
	mov	b, c
	mvi	c, 0
	sui	8
	jz	.Lashl_done
	; fall through

.Lashl_bits:
	mov	l, a		; L = bit count (1-7)
.Lashl_bit_loop:
	mov	a, c
	add	a		; shift left, bit7 → carry
	mov	c, a
	mov	a, b
	adc	a		; shift left + carry
	mov	b, a
	mov	a, e
	adc	a
	mov	e, a
	mov	a, d
	adc	a
	mov	d, a
	dcr	l
	jnz	.Lashl_bit_loop

.Lashl_done:
	ret


; ===================================================================
; __lshrsi3: i32 logical right shift
; ===================================================================
	.section .text.__lshrsi3, "ax", @progbits
	.globl	__lshrsi3
	.type	__lshrsi3,@function
__lshrsi3:
#ifdef UNDOC
	ldsi	6
	ldax	d
#else
	lxi	h, 6
	dad	sp
	mov	a, m
#endif
	ani	0x1F
	lxi	h, 2
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	rz

	cpi	24
	jnc	.Llshr_24
	cpi	16
	jnc	.Llshr_16
	cpi	8
	jnc	.Llshr_8
	jmp	.Llshr_bits

.Llshr_24:
	mov	c, d
	mvi	b, 0
	mvi	e, 0
	mvi	d, 0
	sui	24
	jz	.Llshr_done
	jmp	.Llshr_bits

.Llshr_16:
	mov	c, e
	mov	b, d
	mvi	e, 0
	mvi	d, 0
	sui	16
	jz	.Llshr_done
	jmp	.Llshr_bits

.Llshr_8:
	mov	c, b
	mov	b, e
	mov	e, d
	mvi	d, 0
	sui	8
	jz	.Llshr_done
	; fall through

.Llshr_bits:
	mov	l, a
.Llshr_bit_loop:
	mov	a, d		; byte3 (MSB)
	ana	a		; clear carry
	rar			; shift right, 0 fills MSB
	mov	d, a
	mov	a, e
	rar
	mov	e, a
	mov	a, b
	rar
	mov	b, a
	mov	a, c
	rar
	mov	c, a
	dcr	l
	jnz	.Llshr_bit_loop

.Llshr_done:
	ret


; ===================================================================
; __ashrsi3: i32 arithmetic right shift
; ===================================================================
	.section .text.__ashrsi3, "ax", @progbits
	.globl	__ashrsi3
	.type	__ashrsi3,@function
__ashrsi3:
#ifdef UNDOC
	ldsi	6
	ldax	d
#else
	lxi	h, 6
	dad	sp
	mov	a, m
#endif
	ani	0x1F
	lxi	h, 2
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	rz

	cpi	24
	jnc	.Lashr_24
	cpi	16
	jnc	.Lashr_16
	cpi	8
	jnc	.Lashr_8
	jmp	.Lashr_bits

.Lashr_24:
	sui	24		; remaining count (do before sign ext clobbers A)
	mov	h, a		; save remaining count
	mov	c, d		; byte0 = old byte3
	; Sign extension: 0xFF if negative, 0x00 if positive
	mov	a, d
	add	a		; carry = sign bit
	sbb	a		; A = -carry (0xFF or 0x00)
	mov	b, a
	mov	e, a
	mov	d, a
	mov	a, h		; restore remaining count
	ora	a		; test zero
	jz	.Lashr_done
	jmp	.Lashr_bits

.Lashr_16:
	sui	16
	mov	h, a
	mov	c, e
	mov	b, d
	mov	a, d
	add	a
	sbb	a
	mov	e, a
	mov	d, a
	mov	a, h
	ora	a
	jz	.Lashr_done
	jmp	.Lashr_bits

.Lashr_8:
	sui	8
	mov	h, a
	mov	c, b
	mov	b, e
	mov	e, d
	mov	a, d
	add	a
	sbb	a
	mov	d, a
	mov	a, h
	ora	a
	jz	.Lashr_done
	; fall through

.Lashr_bits:
	mov	l, a
.Lashr_bit_loop:
	; Arithmetic right shift: sign bit replicates into MSB
	mov	a, d		; byte3 (MSB)
	add	a		; carry = sign bit
	mov	a, d		; reload (add clobbered A)
	rar			; shift right, sign fills MSB
	mov	d, a
	mov	a, e
	rar
	mov	e, a
	mov	a, b
	rar
	mov	b, a
	mov	a, c
	rar
	mov	c, a
	dcr	l
	jnz	.Lashr_bit_loop

.Lashr_done:
	ret
