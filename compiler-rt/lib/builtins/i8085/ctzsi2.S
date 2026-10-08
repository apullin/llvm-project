; Hand-optimised __ctzsi2 for i8085.
; Returns the count of trailing zeros in a 32-bit value.
;
; Calling convention:
;   [SP+2..5] = x (i32, little-endian: byte0=LSB at SP+2)
;   Returns i32 in BC:DE (C=byte0, B=byte1, E=byte2, D=byte3)
;
; Algorithm: check bytes from LSB upward. First non-zero byte:
;   compute CTZ of that byte, add byte_index * 8.
;   If all zero, return 32.

	.section .text.__ctzsi2, "ax", @progbits
	.globl	__ctzsi2
	.type	__ctzsi2,@function
__ctzsi2:
	; Byte 0 (LSB) at SP+2
#ifdef UNDOC
	ldsi	2
	ldax	d
#else
	lxi	h, 2
	dad	sp
	mov	a, m
#endif
	ora	a
	jz	.Lctz32_byte1

	; Byte 0 is non-zero, count trailing zeros
	call	.Lctz8
	mov	c, a
	mvi	b, 0
	jmp	.Lctz32_ret

.Lctz32_byte1:
	; Byte 1 at SP+3
	inx	h
	mov	a, m
	ora	a
	jz	.Lctz32_byte2

	call	.Lctz8
	adi	8
	mov	c, a
	mvi	b, 0
	jmp	.Lctz32_ret

.Lctz32_byte2:
	; Byte 2 at SP+4
	inx	h
	mov	a, m
	ora	a
	jz	.Lctz32_byte3

	call	.Lctz8
	adi	16
	mov	c, a
	mvi	b, 0
	jmp	.Lctz32_ret

.Lctz32_byte3:
	; Byte 3 (MSB) at SP+5
	inx	h
	mov	a, m
	ora	a
	jz	.Lctz32_allzero

	call	.Lctz8
	adi	24
	mov	c, a
	mvi	b, 0
	jmp	.Lctz32_ret

.Lctz32_allzero:
	; All bytes zero -> return 32
	mvi	c, 32
	mvi	b, 0

.Lctz32_ret:
	; High half of i32 result is always 0
	xra	a
	mov	d, a
	mov	e, a
	ret

; ctz8: count trailing zeros of byte in A (A must be non-zero).
; Returns count in A (0..7).
.Lctz8:
	mvi	b, 0		; counter (reuse B, will be restored)
.Lctz8_loop:
	mov	c, a		; save A
	ani	0x01		; test LSB
	jnz	.Lctz8_done
	mov	a, c
	ora	a		; clear carry
	rar			; A >>= 1
	inr	b
	jmp	.Lctz8_loop
.Lctz8_done:
	mov	a, b		; A = count
	ret

	.size	__ctzsi2, .-__ctzsi2
