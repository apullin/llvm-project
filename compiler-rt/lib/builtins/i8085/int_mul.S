; Hand-optimised i8085 integer multiply routines.
;
; Replaces the C shift-and-add implementations with register-aware
; assembly that avoids the 32-bit scratch-area overhead.
;
; Calling convention (stack-based, cdecl):
;   Arguments pushed right-to-left.  Each arg occupies its natural
;   size on the stack (uint8_t = 1 byte, uint16_t = 2, uint32_t = 4).
;   8-bit return in A.  16-bit return in BC.  32-bit return in BC:DE
;   (C = byte 0 LSB, B = byte 1, E = byte 2, D = byte 3 MSB).
;
; Algorithm: LSB-first shift-and-add with early termination.
;   while (multiplier != 0):
;     if bit0(multiplier): result += multiplicand
;     multiplicand <<= 1
;     multiplier  >>= 1

; ===================================================================
; uint8_t __mul8(uint8_t a, uint8_t b)
;   [SP+2] = a (1 byte)
;   [SP+3] = b (1 byte)
;   Returns result in A (also in C; B = 0).
; ===================================================================
	.section .text.__mul8, "ax", @progbits
	.globl	__mul8
	.type	__mul8,@function
__mul8:
	; Load a (multiplicand) into E
	lxi	h, 2
	dad	sp
	mov	e, m

	; Load b (multiplier) into D
	lxi	h, 3
	dad	sp
	mov	d, m

	; Result accumulator in C
	mvi	c, 0

.Lm8_loop:
	; Early exit when multiplier is exhausted
	mov	a, d
	ora	a
	jz	.Lm8_done

	; Test LSB of multiplier
	ani	1
	jz	.Lm8_no_add
	mov	a, c
	add	e		; result += multiplicand
	mov	c, a
.Lm8_no_add:
	; multiplicand <<= 1
	mov	a, e
	add	a
	mov	e, a

	; multiplier >>= 1
	mov	a, d
	ora	a		; clear carry
	rar
	mov	d, a

	jmp	.Lm8_loop

.Lm8_done:
	mov	a, c
	mvi	b, 0
	ret
	.size	__mul8, .-__mul8


; ===================================================================
; uint16_t __mul16(uint16_t a, uint16_t b)
;   [SP+2..3] = a
;   [SP+4..5] = b
;   Returns result in BC (B = high, C = low).
; ===================================================================
	.section .text.__mul16, "ax", @progbits
	.globl	__mul16
	.type	__mul16,@function
__mul16:
	; Load a (multiplicand) into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Load b (multiplier) into BC
	lxi	h, 4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m

	; HL = 0 (result accumulator)
	lxi	h, 0

.Lm16_loop:
	; Early exit when multiplier == 0
	mov	a, b
	ora	c
	jz	.Lm16_done

	; Test LSB of multiplier
	mov	a, c
	ani	1
	jz	.Lm16_no_add
	dad	d		; result += multiplicand (16-bit)
.Lm16_no_add:
	; multiplier >>= 1  (BC >>= 1)
	mov	a, b
	ora	a		; clear carry
	rar			; B >>= 1
	mov	b, a
	mov	a, c
	rar			; C >>= 1, old B.bit0 into C.bit7
	mov	c, a

	; multiplicand <<= 1  (DE <<= 1)
	mov	a, e
	add	a		; E <<= 1
	mov	e, a
	mov	a, d
	adc	a		; D = D*2 + carry
	mov	d, a

	jmp	.Lm16_loop

.Lm16_done:
	; Move result from HL to BC
	mov	b, h
	mov	c, l
	ret
	.size	__mul16, .-__mul16


; ===================================================================
; uint32_t __mul32(uint32_t a, uint32_t b)
;   [SP+2..5]  = a  (4 bytes, little-endian)
;   [SP+6..9]  = b  (4 bytes, little-endian)
;   Returns in BC:DE  (C=byte0, B=byte1, E=byte2, D=byte3).
;
; Strategy: keep result in registers BC:DE (no stack allocation).
; Use a as the multiplier (shift right, test LSB, early exit).
; Use b as the multiplicand (shift left each iteration).
; Both a and b are modified in-place on the caller's stack frame
; (permitted: caller owns that memory after pushing args).
;
; The result accumulation uses register-to-memory adds (ADD M /
; ADC M) instead of loading the multiplicand into registers and
; then adding byte-by-byte through a stack-based result.  This
; saves ~9 instructions and ~78 cycles per add iteration.
; ===================================================================
	.section .text.__mul32, "ax", @progbits
	.globl	__mul32
	.type	__mul32,@function
__mul32:
	; Result lives in BC:DE -- no stack allocation needed.
	lxi	b, 0		; result bytes 0-1 (C=byte0, B=byte1)
	lxi	d, 0		; result bytes 2-3 (E=byte2, D=byte3)

	; Stack layout (offsets from current SP):
	;   [SP+0..1] = return address
	;   [SP+2..5] = a  (multiplier,    shifts right)
	;   [SP+6..9] = b  (multiplicand, shifts left)

.Lm32_loop:
	; --- Check if multiplier (a) == 0 ---
	lxi	h, 2
	dad	sp
	mov	a, m		; a[0]
	inx	h
	ora	m		; | a[1]
	inx	h
	ora	m		; | a[2]
	inx	h
	ora	m		; | a[3]
	jz	.Lm32_done

	; --- Test bit 0 of multiplier ---
	; HL currently -> a[3]; walk back to a[0] (DCX is 6 cyc vs
	; LXI+DAD SP = 20 cyc, saves 2 cycles per iteration).
	dcx	h		; -> a[2]
	dcx	h		; -> a[1]
	dcx	h		; -> a[0]
	mov	a, m
	ani	1
	jz	.Lm32_no_add

	; --- result (BC:DE) += multiplicand (on stack) ---
	; Add b[0..3] directly from memory into register result.
	lxi	h, 6
	dad	sp		; HL -> b[0]

	mov	a, c
	add	m		; C += b[0]
	mov	c, a

	inx	h		; -> b[1]
	mov	a, b
	adc	m		; B += b[1] + carry
	mov	b, a

	inx	h		; -> b[2]
	mov	a, e
	adc	m		; E += b[2] + carry
	mov	e, a

	inx	h		; -> b[3]
	mov	a, d
	adc	m		; D += b[3] + carry
	mov	d, a

.Lm32_no_add:
	; --- Shift multiplier (a) right by 1 ---
	; Process MSB first so carry propagates downward.
	lxi	h, 5
	dad	sp		; HL -> a[3]
	mov	a, m
	ora	a		; clear carry
	rar
	mov	m, a

	dcx	h		; -> a[2]
	mov	a, m
	rar
	mov	m, a

	dcx	h		; -> a[1]
	mov	a, m
	rar
	mov	m, a

	dcx	h		; -> a[0]
	mov	a, m
	rar
	mov	m, a

	; --- Shift multiplicand (b) left by 1 ---
	; Process LSB first so carry propagates upward.
	lxi	h, 6
	dad	sp		; HL -> b[0]
	mov	a, m
	add	a		; b[0] <<= 1
	mov	m, a

	inx	h		; -> b[1]
	mov	a, m
	adc	a
	mov	m, a

	inx	h		; -> b[2]
	mov	a, m
	adc	a
	mov	m, a

	inx	h		; -> b[3]
	mov	a, m
	adc	a
	mov	m, a

	jmp	.Lm32_loop

.Lm32_done:
	; Result is already in BC:DE (return convention).
	ret
	.size	__mul32, .-__mul32


; ===================================================================
; int32_t __mulsi16(int16_t a, int16_t b)
;   [SP+2..3] = a  (first arg, 16-bit signed)
;   [SP+4..5] = b  (second arg, 16-bit signed)
;   Returns signed 32-bit product in BC:DE
;   (C=byte0 LSB, B=byte1, E=byte2, D=byte3 MSB).
;
; Strategy: determine result sign from arg signs, negate negative
; args, do unsigned 16x16->32 multiply, negate result if needed.
;
; Optimised: result lives in BC:DE (register-resident).
; Multiplier at top of stack, accessed via XTHL (16 cyc) to swap
; into/out of HL without affecting flags.  This avoids the
; push/pop multiplier + two LXI+DAD SP pairs that the old
; stack-resident-result approach needed on every add iteration.
; ===================================================================
	.section .text.__mulsi16, "ax", @progbits
	.globl	__mulsi16
	.type	__mulsi16,@function
__mulsi16:
	; Load a into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Load b into BC (temporarily)
	lxi	h, 4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m

	; Compute result sign: bit7 of (high_a XOR high_b)
	mov	a, d
	xra	b
	push	psw		; save sign flag on stack

	; Make a (DE) unsigned: if negative, negate
	mov	a, d
	ora	a
	jp	.Lms16_a_pos
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lms16_a_pos:

	; Make b (BC) unsigned: if negative, negate
	mov	a, b
	ora	a
	jp	.Lms16_b_pos
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lms16_b_pos:

	; DE = |a| (multiplier), BC = |b| (multiplicand)
	; Allocate multiplicand on stack (4 bytes, zero-extended)
	lxi	h, 0
	push	h		; mcand[2..3] = 0
	push	b		; mcand[0..1] = |b|
	; Push multiplier to top of stack (for XTHL access)
	push	d		; [SP+0..1] = multiplier

	; Stack layout (offsets from current SP):
	;   [SP+0..1]  = multiplier (accessed via XTHL)
	;   [SP+2..5]  = multiplicand (|b|, zero-extended to 32-bit)
	;   [SP+6..7]  = sign flag (PSW push)
	;   [SP+8..9]  = return address
	;   [SP+10..11] = a (original arg)
	;   [SP+12..13] = b (original arg)

	; Result lives in BC:DE -- register-resident.
	lxi	b, 0		; result bytes 0-1 (C=byte0, B=byte1)
	lxi	d, 0		; result bytes 2-3 (E=byte2, D=byte3)

.Lms16_loop:
	; --- Load multiplier into HL, check if zero ---
	xthl			; HL = multiplier, [SP] = junk (old HL)
	mov	a, h
	ora	l
	jz	.Lms16_mul_done

	; --- Shift multiplier right by 1, capture old bit0 in carry ---
	mov	a, h
	ora	a		; clear carry
	rar
	mov	h, a
	mov	a, l
	rar
	mov	l, a
	; carry = old bit 0 of multiplier

	; --- Save shifted multiplier back to stack ---
	xthl			; [SP] = shifted multiplier, HL = junk
	; (XTHL does not affect flags -- carry is preserved)

	; --- Branch: skip add if old bit 0 was 0 ---
	jnc	.Lms16_no_add

	; --- result (BC:DE) += multiplicand (on stack) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]

	mov	a, c
	add	m		; C += mcand[0]
	mov	c, a

	inx	h		; -> mcand[1]
	mov	a, b
	adc	m		; B += mcand[1] + carry
	mov	b, a

	inx	h		; -> mcand[2]
	mov	a, e
	adc	m		; E += mcand[2] + carry
	mov	e, a

	inx	h		; -> mcand[3]
	mov	a, d
	adc	m		; D += mcand[3] + carry
	mov	d, a

.Lms16_no_add:
	; --- Shift multiplicand left by 1 (4 bytes on stack) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]
	mov	a, m
	add	a		; mcand[0] <<= 1
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

	jmp	.Lms16_loop

.Lms16_mul_done:
	; HL = 0 (multiplier was zero). [SP] has junk from XTHL.
	; Result is already in BC:DE.
	; Deallocate multiplier + multiplicand (6 bytes)
	lxi	h, 6
	dad	sp
	sphl

	; Pop sign flag
	pop	psw
	ora	a		; test bit 7 (sign)
	jp	.Lms16_pos_result

	; Negate 32-bit result in BC:DE (C=byte0, B=byte1, E=byte2, D=byte3)
	; result = ~result + 1
	mov	a, c
	cma
	adi	1		; byte0 = ~byte0 + 1
	mov	c, a
	mov	a, b
	cma
	aci	0		; byte1 = ~byte1 + carry
	mov	b, a
	mov	a, e
	cma
	aci	0		; byte2 = ~byte2 + carry
	mov	e, a
	mov	a, d
	cma
	aci	0		; byte3 = ~byte3 + carry
	mov	d, a

.Lms16_pos_result:
	ret
	.size	__mulsi16, .-__mulsi16


; ===================================================================
; int16_t __mulsi16_shr8(int16_t a, int16_t b)
;   [SP+2..3] = a  (first arg, 16-bit signed)
;   [SP+4..5] = b  (second arg, 16-bit signed)
;   Returns (int16_t)( ((int32_t)a * (int32_t)b) >> 8 )
;   i.e. bytes [1..2] of the 32-bit product.
;   Result in BC (B = high, C = low).
;
; Optimisation: we need bytes 1 and 2 of the product.
; Byte 0 can be skipped (only its carry into byte 1 matters).
; Byte 3 is not needed.  So we accumulate a 3-byte result
; (bytes 0, 1, 2) in registers and return bytes 1-2.
;
; Register-resident result: C=byte0, B=byte1, D=byte2.
; Multiplier at top of stack, accessed via XTHL.
; ===================================================================
	.section .text.__mulsi16_shr8, "ax", @progbits
	.globl	__mulsi16_shr8
	.type	__mulsi16_shr8,@function
__mulsi16_shr8:
	; Load a into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Load b into BC
	lxi	h, 4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m

	; Compute result sign
	mov	a, d
	xra	b
	push	psw		; save sign flag

	; Make a (DE) unsigned
	mov	a, d
	ora	a
	jp	.Lms16s8_a_pos
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lms16s8_a_pos:

	; Make b (BC) unsigned
	mov	a, b
	ora	a
	jp	.Lms16s8_b_pos
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lms16s8_b_pos:

	; DE = |a| (multiplier), BC = |b| (multiplicand)
	; Allocate multiplicand on stack (4 bytes, zero-extended, only 3 used)
	lxi	h, 0
	push	h		; mcand[2..3] = 0
	push	b		; mcand[0..1] = |b|
	; Push multiplier to top of stack (for XTHL access)
	push	d		; [SP+0..1] = multiplier

	; Stack layout:
	;   [SP+0..1]  = multiplier (accessed via XTHL)
	;   [SP+2..5]  = multiplicand (|b|, zero-extended)
	;   [SP+6..7]  = sign flag
	;   [SP+8..9]  = return address

	; Result in registers: C=byte0, B=byte1, D=byte2
	mvi	c, 0
	mvi	b, 0
	mvi	d, 0

.Lms16s8_loop:
	; --- Load multiplier into HL, check if zero ---
	xthl			; HL = multiplier, [SP] = junk
	mov	a, h
	ora	l
	jz	.Lms16s8_mul_done

	; --- Shift multiplier right by 1, capture old bit0 in carry ---
	mov	a, h
	ora	a		; clear carry
	rar
	mov	h, a
	mov	a, l
	rar
	mov	l, a
	; carry = old bit 0 of multiplier

	; --- Save shifted multiplier back ---
	xthl			; [SP] = shifted multiplier, HL = junk
	; (XTHL does not affect flags)

	jnc	.Lms16s8_no_add

	; --- result (C:B:D) += mcand[0..2] from stack ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]

	mov	a, c
	add	m		; C += mcand[0]
	mov	c, a

	inx	h		; -> mcand[1]
	mov	a, b
	adc	m		; B += mcand[1] + carry
	mov	b, a

	inx	h		; -> mcand[2]
	mov	a, d
	adc	m		; D += mcand[2] + carry
	mov	d, a

.Lms16s8_no_add:
	; --- Shift multiplicand left by 1 (3 bytes: mcand[0..2]) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]
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

	jmp	.Lms16s8_loop

.Lms16s8_mul_done:
	; HL = 0 (multiplier was zero).
	; Result is in C=byte0, B=byte1, D=byte2.
	; Deallocate multiplier + multiplicand (6 bytes)
	lxi	h, 6
	dad	sp
	sphl

	; Pop sign flag
	pop	psw
	ora	a
	jp	.Lms16s8_pos_result

	; Negate 3-byte result in C:B:D (C=byte0, B=byte1, D=byte2)
	; Then return bytes 1-2 (B and D)
	mov	a, c
	cma
	adi	1		; ~byte0 + 1
	mov	a, b
	cma
	aci	0		; ~byte1 + carry
	mov	c, a		; C = negated byte1 (return low)
	mov	a, d
	cma
	aci	0		; ~byte2 + carry
	mov	b, a		; B = negated byte2 (return high)
	ret

.Lms16s8_pos_result:
	; Return bytes 1-2: B=byte2 (high), C=byte1 (low)
	mov	c, b		; C = byte1
	mov	b, d		; B = byte2
	ret
	.size	__mulsi16_shr8, .-__mulsi16_shr8


; ===================================================================
; int16_t __mulsi16_hi16(int16_t a, int16_t b)
;   [SP+2..3] = a
;   [SP+4..5] = b
;   Returns upper 16 bits of signed 32-bit product: bits [31:16].
;   Result in BC.
;
; Optimised: register-resident result in BC:DE.
; Multiplier at top of stack, accessed via XTHL.
; Same core loop as __mulsi16 but returns bytes 2-3 (E:D -> C:B).
; ===================================================================
	.section .text.__mulsi16_hi16, "ax", @progbits
	.globl	__mulsi16_hi16
	.type	__mulsi16_hi16,@function
__mulsi16_hi16:
	; Load a into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Load b into BC
	lxi	h, 4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m

	; Compute result sign
	mov	a, d
	xra	b
	push	psw

	; Make a (DE) unsigned
	mov	a, d
	ora	a
	jp	.Lms16h_a_pos
	mov	a, e
	cma
	mov	e, a
	mov	a, d
	cma
	mov	d, a
	inx	d
.Lms16h_a_pos:

	; Make b (BC) unsigned
	mov	a, b
	ora	a
	jp	.Lms16h_b_pos
	mov	a, c
	cma
	mov	c, a
	mov	a, b
	cma
	mov	b, a
	inx	b
.Lms16h_b_pos:

	; DE = |a| (multiplier), BC = |b| (multiplicand)
	; Allocate multiplicand on stack (4 bytes, zero-extended)
	lxi	h, 0
	push	h		; mcand[2..3] = 0
	push	b		; mcand[0..1] = |b|
	; Push multiplier to top of stack (for XTHL access)
	push	d		; [SP+0..1] = multiplier

	; Stack layout:
	;   [SP+0..1]  = multiplier (accessed via XTHL)
	;   [SP+2..5]  = multiplicand (|b|, zero-extended)
	;   [SP+6..7]  = sign flag
	;   [SP+8..9]  = return address

	; Result in BC:DE (register-resident)
	lxi	b, 0		; C=byte0, B=byte1
	lxi	d, 0		; E=byte2, D=byte3

.Lms16h_loop:
	; --- Load multiplier into HL, check if zero ---
	xthl			; HL = multiplier, [SP] = junk
	mov	a, h
	ora	l
	jz	.Lms16h_mul_done

	; --- Shift multiplier right by 1, capture old bit0 in carry ---
	mov	a, h
	ora	a		; clear carry
	rar
	mov	h, a
	mov	a, l
	rar
	mov	l, a
	; carry = old bit 0 of multiplier

	; --- Save shifted multiplier back ---
	xthl			; [SP] = shifted multiplier, HL = junk
	; (XTHL does not affect flags)

	jnc	.Lms16h_no_add

	; --- result (BC:DE) += multiplicand (on stack) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]

	mov	a, c
	add	m		; C += mcand[0]
	mov	c, a

	inx	h		; -> mcand[1]
	mov	a, b
	adc	m		; B += mcand[1] + carry
	mov	b, a

	inx	h		; -> mcand[2]
	mov	a, e
	adc	m		; E += mcand[2] + carry
	mov	e, a

	inx	h		; -> mcand[3]
	mov	a, d
	adc	m		; D += mcand[3] + carry
	mov	d, a

.Lms16h_no_add:
	; --- Shift multiplicand left by 1 (4 bytes) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]
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
	mov	m, a

	jmp	.Lms16h_loop

.Lms16h_mul_done:
	; HL = 0 (multiplier was zero).
	; Result is in BC:DE.
	; Deallocate multiplier + multiplicand (6 bytes)
	lxi	h, 6
	dad	sp
	sphl

	pop	psw
	ora	a
	jp	.Lms16h_pos_result

	; Negate full 32-bit result to get bytes 2-3 of signed product
	mov	a, c
	cma
	adi	1		; ~byte0 + 1
	mov	a, b
	cma
	aci	0		; ~byte1 + carry
	mov	a, e
	cma
	aci	0		; ~byte2 + carry
	mov	c, a		; C = negated byte2 (return low)
	mov	a, d
	cma
	aci	0		; ~byte3 + carry
	mov	b, a		; B = negated byte3 (return high)
	ret

.Lms16h_pos_result:
	; Return bytes 2-3: C=byte2 (low), B=byte3 (high)
	mov	c, e
	mov	b, d
	ret
	.size	__mulsi16_hi16, .-__mulsi16_hi16


; ===================================================================
; int16_t __mulsi16_lo16(int16_t a, int16_t b)
;   [SP+2..3] = a
;   [SP+4..5] = b
;   Returns lower 16 bits of signed 32-bit product: bits [15:0].
;   Result in BC.
;
; Note: the lower 16 bits of a signed multiply equal the lower 16
; bits of the unsigned multiply.  No sign handling is needed!
; This is just __mul16 renamed.  We can skip computing bytes 2-3.
; ===================================================================
	.section .text.__mulsi16_lo16, "ax", @progbits
	.globl	__mulsi16_lo16
	.type	__mulsi16_lo16,@function
__mulsi16_lo16:
	; Load a (multiplicand) into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Load b (multiplier) into BC
	lxi	h, 4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m

	; HL = 0 (result accumulator, 16-bit)
	lxi	h, 0

.Lms16lo_loop:
	mov	a, b
	ora	c
	jz	.Lms16lo_done

	mov	a, c
	ani	1
	jz	.Lms16lo_no_add
	dad	d		; result += multiplicand (16-bit)
.Lms16lo_no_add:
	; multiplier >>= 1 (BC >>= 1)
	mov	a, b
	ora	a
	rar
	mov	b, a
	mov	a, c
	rar
	mov	c, a

	; multiplicand <<= 1 (DE <<= 1)
	mov	a, e
	add	a
	mov	e, a
	mov	a, d
	adc	a
	mov	d, a

	jmp	.Lms16lo_loop

.Lms16lo_done:
	mov	b, h
	mov	c, l
	ret
	.size	__mulsi16_lo16, .-__mulsi16_lo16


; ===================================================================
; int16_t __mulsi8(int8_t a, int8_t b)
;   [SP+2] = a (1 byte, signed)
;   [SP+3] = b (1 byte, signed)
;   Returns signed 16-bit product in BC (B = high, C = low).
;
; Strategy: determine result sign from arg signs, negate negative
; args, do unsigned 8x8->16 shift-and-add (max 8 iterations),
; negate result if needed.
;
; Registers during multiply core:
;   D  = multiplier  (|a|, shifts right, 8-bit)
;   E  = multiplicand low byte (|b|, shifts left into DE-style)
;   HL = 16-bit result accumulator
;   B  = multiplicand high byte (starts 0, grows as E shifts out)
; ===================================================================
	.section .text.__mulsi8, "ax", @progbits
	.globl	__mulsi8
	.type	__mulsi8,@function
__mulsi8:
	; Load a into D
	lxi	h, 2
	dad	sp
	mov	d, m

	; Load b into E
	lxi	h, 3
	dad	sp
	mov	e, m

	; Compute result sign: bit7 of (a XOR b)
	mov	a, d
	xra	e
	push	psw		; save sign flag on stack

	; Make a (D) unsigned: if negative, negate
	mov	a, d
	ora	a
	jp	.Lms8_a_pos
	cma
	adi	1
	mov	d, a
.Lms8_a_pos:

	; Make b (E) unsigned: if negative, negate
	mov	a, e
	ora	a
	jp	.Lms8_b_pos
	cma
	adi	1
	mov	e, a
.Lms8_b_pos:

	; D = |a| (multiplier), E = |b| (multiplicand low byte)
	; B = multiplicand high byte (starts 0)
	; HL = result accumulator (starts 0)
	mvi	b, 0
	lxi	h, 0

.Lms8_loop:
	; Early exit when multiplier is exhausted
	mov	a, d
	ora	a
	jz	.Lms8_mul_done

	; Test LSB of multiplier
	ani	1
	jz	.Lms8_no_add

	; result (HL) += multiplicand (B:E as 16-bit, B=high, E=low)
	; Build multiplicand in a temporary pair for DAD:
	; We need to add B:E to HL.  Use: A=L+E, L=A; A=H+B+carry, H=A
	mov	a, l
	add	e
	mov	l, a
	mov	a, h
	adc	b
	mov	h, a

.Lms8_no_add:
	; multiplicand <<= 1  (B:E <<= 1)
	mov	a, e
	add	a		; E <<= 1
	mov	e, a
	mov	a, b
	adc	a		; B = B*2 + carry
	mov	b, a

	; multiplier >>= 1
	mov	a, d
	ora	a		; clear carry
	rar
	mov	d, a

	jmp	.Lms8_loop

.Lms8_mul_done:
	; Result is in HL, move to BC
	mov	b, h
	mov	c, l

	; Pop sign flag
	pop	psw
	ora	a		; test bit 7 (sign)
	jp	.Lms8_pos_result

	; Negate 16-bit result in BC
	mov	a, c
	cma
	adi	1		; ~low + 1
	mov	c, a
	mov	a, b
	cma
	aci	0		; ~high + carry
	mov	b, a

.Lms8_pos_result:
	ret
	.size	__mulsi8, .-__mulsi8


; ===================================================================
; int8_t __mulsi8_hi8(int8_t a, int8_t b)
;   [SP+2] = a (1 byte, signed)
;   [SP+3] = b (1 byte, signed)
;   Returns upper 8 bits of signed 8x8->16 product.
;   i.e. byte 1 of the 16-bit product (bits [15:8]).
;   Returns i8 in A (and in C with B=0 for i16 compat).
;   This is MULHS for i8.
;
; Strategy: call into __mulsi8 logic (sign, unsigned core, fix sign),
; then extract byte 1 (B register after __mulsi8 returns in BC).
; For code size, we just call __mulsi8 and extract B.
; ===================================================================
	.section .text.__mulsi8_hi8, "ax", @progbits
	.globl	__mulsi8_hi8
	.type	__mulsi8_hi8,@function
__mulsi8_hi8:
	; Load a into D
	lxi	h, 2
	dad	sp
	mov	d, m

	; Load b into E
	lxi	h, 3
	dad	sp
	mov	e, m

	; Compute result sign: bit7 of (a XOR b)
	mov	a, d
	xra	e
	push	psw		; save sign flag on stack

	; Make a (D) unsigned
	mov	a, d
	ora	a
	jp	.Lms8h_a_pos
	cma
	adi	1
	mov	d, a
.Lms8h_a_pos:

	; Make b (E) unsigned
	mov	a, e
	ora	a
	jp	.Lms8h_b_pos
	cma
	adi	1
	mov	e, a
.Lms8h_b_pos:

	; D = |a| (multiplier), E = |b| (multiplicand low)
	; B = multiplicand high (starts 0)
	; HL = result accumulator
	mvi	b, 0
	lxi	h, 0

.Lms8h_loop:
	mov	a, d
	ora	a
	jz	.Lms8h_mul_done

	ani	1
	jz	.Lms8h_no_add

	; HL += B:E
	mov	a, l
	add	e
	mov	l, a
	mov	a, h
	adc	b
	mov	h, a

.Lms8h_no_add:
	; multiplicand <<= 1
	mov	a, e
	add	a
	mov	e, a
	mov	a, b
	adc	a
	mov	b, a

	; multiplier >>= 1
	mov	a, d
	ora	a
	rar
	mov	d, a

	jmp	.Lms8h_loop

.Lms8h_mul_done:
	; Result is in HL (H=high byte, L=low byte)
	; We need the high byte (H), but must apply sign to full 16-bit first.
	mov	b, h
	mov	c, l

	; Pop sign flag
	pop	psw
	ora	a
	jp	.Lms8h_pos_result

	; Negate 16-bit result in BC, then extract B (high byte)
	mov	a, c
	cma
	adi	1
	mov	a, b
	cma
	aci	0
	mov	b, a		; B = negated high byte

	; Return high byte in A (and C for i16 compat with B=0)
	mov	a, b
	mov	c, b
	mvi	b, 0
	ret

.Lms8h_pos_result:
	; Return high byte (H) in A
	mov	a, h
	mov	c, h
	mvi	b, 0
	ret
	.size	__mulsi8_hi8, .-__mulsi8_hi8


; ===================================================================
; int8_t __mulsi8_lo8(int8_t a, int8_t b)
;   [SP+2] = a (1 byte, signed)
;   [SP+3] = b (1 byte, signed)
;   Returns lower 8 bits of signed 8x8->16 product.
;   Since low 8 bits of signed multiply == low 8 bits of unsigned
;   multiply, no sign handling is needed.  This is just __mul8.
;   Returns i8 in A (also in C; B = 0).
; ===================================================================
	.section .text.__mulsi8_lo8, "ax", @progbits
	.globl	__mulsi8_lo8
	.type	__mulsi8_lo8,@function
__mulsi8_lo8:
	; Tail-call into __mul8 which has the same calling convention
	; and returns the low 8 bits of the unsigned product.
	jmp	__mul8
	.size	__mulsi8_lo8, .-__mulsi8_lo8


; ===================================================================
; int64_t __mulsi32(int32_t a, int32_t b)
;   Called via sret convention for i64 return:
;   [SP+2..3] = sret pointer (where to store 8-byte result)
;   [SP+4..7] = a  (4 bytes, little-endian, signed)
;   [SP+8..11] = b  (4 bytes, little-endian, signed)
;   Returns: void (writes 8 bytes to sret pointer)
;
; Strategy: determine result sign from arg signs, negate negative
; inputs, do unsigned 32x32->64 shift-and-add, negate result if
; needed, write to sret pointer.
;
; Stack frame for the unsigned core:
;   - 8 bytes for the 64-bit result (on stack, zeroed)
;   - multiplier = a (in-place on caller's stack, shifts right)
;   - multiplicand = b (8 bytes on our stack, zero-extended to 64 bits,
;     shifts left each iteration)
; ===================================================================
	.section .text.__mulsi32, "ax", @progbits
	.globl	__mulsi32
	.type	__mulsi32,@function
__mulsi32:
	; Compute result sign: XOR sign bytes of a and b
	; a[3] is at [SP+7], b[3] is at [SP+11]
#ifdef UNDOC
	ldsi	7
	ldax	d		; a[3] (sign byte of a)
#else
	lxi	h, 7
	dad	sp
	mov	a, m		; a[3] (sign byte of a)
#endif
	lxi	h, 11
	dad	sp
	xra	m		; XOR with b[3]
	push	psw		; save sign flag (bit 7 = result is negative)

	; --- Make a (multiplier) unsigned ---
	; a is at [SP+6..9] (pushed sign +2)
#ifdef UNDOC
	ldsi	9
	ldax	d
#else
	lxi	h, 9
	dad	sp		; -> a[3]
	mov	a, m
#endif
	ora	a
	jp	.Lms32_a_pos

	; Negate 32-bit a in-place: ~a + 1
	lxi	h, 6
	dad	sp		; -> a[0]
	mov	a, m
	cma
	adi	1
	mov	m, a
	inx	h		; -> a[1]
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h		; -> a[2]
	mov	a, m
	cma
	aci	0
	mov	m, a
	inx	h		; -> a[3]
	mov	a, m
	cma
	aci	0
	mov	m, a
.Lms32_a_pos:

	; --- Make b (multiplicand) unsigned ---
	; b is at [SP+10..13] (pushed sign +2)
#ifdef UNDOC
	ldsi	13
	ldax	d
#else
	lxi	h, 13
	dad	sp		; -> b[3]
	mov	a, m
#endif
	ora	a
	jp	.Lms32_b_pos

	; Negate 32-bit b in-place
	lxi	h, 10
	dad	sp		; -> b[0]
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
.Lms32_b_pos:

	; --- Allocate multiplicand (8 bytes: b zero-extended to 64 bits) ---
	; mcand[0..3] = |b|, mcand[4..7] = 0
	lxi	h, 0
	push	h		; mcand[6..7] = 0
	push	h		; mcand[4..5] = 0

	; Load |b| from [SP+14..17] (sign +2, mcand +4 = +6 from original +8)
	lxi	h, 14
	dad	sp		; -> b[0]
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d		; mcand[2..3] = b[2..3]
	push	b		; mcand[0..1] = b[0..1]

	; --- Allocate 8-byte result, zeroed ---
	lxi	h, 0
	push	h		; result[6..7] = 0
	push	h		; result[4..5] = 0
	push	h		; result[2..3] = 0
	push	h		; result[0..1] = 0

	; Stack layout (offsets from current SP):
	;   [SP+ 0.. 1] = result[0..1]
	;   [SP+ 2.. 3] = result[2..3]
	;   [SP+ 4.. 5] = result[4..5]
	;   [SP+ 6.. 7] = result[6..7]
	;   [SP+ 8.. 9] = mcand[0..1]
	;   [SP+10..11] = mcand[2..3]
	;   [SP+12..13] = mcand[4..5]
	;   [SP+14..15] = mcand[6..7]
	;   [SP+16..17] = sign flag (PSW push)
	;   [SP+18..19] = return address
	;   [SP+20..21] = sret pointer
	;   [SP+22..25] = a (multiplier, shifts right)
	;   [SP+26..29] = b (original, now |b|)

.Lms32_loop:
	; --- Check if multiplier (a) == 0 ---
	lxi	h, 22
	dad	sp
	mov	a, m		; a[0]
	inx	h
	ora	m		; | a[1]
	inx	h
	ora	m		; | a[2]
	inx	h
	ora	m		; | a[3]
	jz	.Lms32_mul_done

	; --- Test bit 0 of multiplier ---
	lxi	h, 22
	dad	sp
	mov	a, m
	ani	1
	jz	.Lms32_no_add

	; --- result += multiplicand (8-byte add) ---
	; Load mcand[0..3] into registers, save pointer to mcand[4],
	; then add to result[0..7] with carry chain preserved.
	; IMPORTANT: DAD SP destroys carry flag, so we save the mcand[4]
	; pointer via push h / pop h (which does NOT affect flags).
	lxi	h, 8
	dad	sp		; -> mcand[0]
	mov	c, m		; mcand[0]
	inx	h
	mov	b, m		; mcand[1]
	inx	h
	mov	e, m		; mcand[2]
	inx	h
	mov	d, m		; mcand[3]
	inx	h		; -> mcand[4]
	push	h		; save pointer to mcand[4] (SP shifts +2)

	; Add to result[0..3]
	lxi	h, 2
	dad	sp		; -> result[0] (shifted +2 for push)
	mov	a, m
	add	c
	mov	m, a		; result[0] += mcand[0]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[1] += mcand[1] + carry
	inx	h
	mov	a, m
	adc	e
	mov	m, a		; result[2] += mcand[2] + carry
	inx	h
	mov	a, m
	adc	d
	mov	m, a		; result[3] += mcand[3] + carry

	; Carry from result[3] is live. Now we need mcand[4..7].
	; HL currently points to result[3]. Save result[4] pointer.
	inx	h		; -> result[4]
	mov	d, h
	mov	e, l		; DE = &result[4]

	pop	h		; HL = &mcand[4] (carry flag preserved!)
	mov	c, m		; mcand[4]
	inx	h
	mov	b, m		; mcand[5]
	inx	h
	push	h		; save pointer to mcand[6] (SP shifts +2)

	xchg			; HL = &result[4], DE saved
	mov	a, m
	adc	c
	mov	m, a		; result[4] += mcand[4] + carry
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[5] += mcand[5] + carry

	; Carry from result[5] is live. Need mcand[6..7].
	inx	h		; -> result[6]
	mov	d, h
	mov	e, l		; DE = &result[6]

	pop	h		; HL = &mcand[6] (carry flag preserved!)
	mov	c, m		; mcand[6]
	inx	h
	mov	b, m		; mcand[7]

	xchg			; HL = &result[6]
	mov	a, m
	adc	c
	mov	m, a		; result[6] += mcand[6] + carry
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[7] += mcand[7] + carry

.Lms32_no_add:
	; --- Shift multiplier (a) right by 1 ---
	lxi	h, 25
	dad	sp		; -> a[3]
	mov	a, m
	ora	a		; clear carry
	rar
	mov	m, a
	dcx	h		; -> a[2]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[1]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[0]
	mov	a, m
	rar
	mov	m, a

	; --- Shift multiplicand (mcand) left by 1 (8 bytes) ---
	lxi	h, 8
	dad	sp		; -> mcand[0]
	mov	a, m
	add	a		; mcand[0] <<= 1
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[1]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[2]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[3]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[4]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[5]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[6]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[7]

	jmp	.Lms32_loop

.Lms32_mul_done:
	; Result is in stack [SP+0..7].
	; Check sign and negate if needed, then write to sret pointer.

	; Deallocate mcand (8 bytes) first to simplify offsets.
	; New layout after removing mcand:
	;   result is [SP+0..7], sign at [SP+8..9], ret at [SP+10..11],
	;   sret at [SP+12..13]

	; Actually, let's read result first, then clean up.
	; Load result bytes 0-7 in two batches.

	; Pop sign flag first, but we need result. Let's do:
	; 1. Read sret pointer
	; 2. Read result + apply sign
	; 3. Write to sret
	; 4. Clean up stack

	; Read sret pointer into DE
	lxi	h, 20
	dad	sp
	mov	e, m		; sret_lo
	inx	h
	mov	d, m		; sret_hi

	; Push sret pointer for later
	push	d		; SP shifts +2

	; Now read sign flag
	lxi	h, 18		; sign at [SP+16+2]
	dad	sp
	mov	a, m		; sign byte (PSW with flags)
	ora	a		; test bit 7
	jp	.Lms32_write_pos

	; --- Negate 64-bit result in-place on stack ---
	; result is at [SP+2..9] (shifted +2 by push d)
	lxi	h, 2
	dad	sp		; -> result[0]
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

.Lms32_write_pos:
	; Pop sret pointer back into DE
	pop	d		; SP shifts -2, back to original

	; Write result[0..7] to sret pointer (DE = ptr)
	; result is at [SP+0..7]
	lxi	h, 0
	dad	sp

	; Byte 0
	mov	a, m
	stax	d		; [sret+0] = result[0]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+1] = result[1]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+2] = result[2]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+3] = result[3]
	inx	h
	inx	d

	; Can't use STAX with HL, need to use memory moves.
	; DE now points to sret+4. HL points to result[4].
	; We need to copy bytes 4-7 but STAX only works with BC and DE.
	; Save DE (sret+4), use BC for the remaining stores.
	push	d		; save sret+4 on stack (SP+2 shift)

	; Load result[4..7] into registers
	; HL currently -> result[4] but SP shifted, so need +2
	lxi	h, 6		; result[4] at SP+4+2=6
	dad	sp
	mov	c, m		; result[4]
	inx	h
	mov	b, m		; result[5]
	inx	h
	mov	e, m		; result[6]
	inx	h
	mov	d, m		; result[7]

	; Retrieve sret+4 into HL
	pop	h		; HL = sret+4 (SP-2 shift, back to normal)

	mov	m, c		; [sret+4] = result[4]
	inx	h
	mov	m, b		; [sret+5] = result[5]
	inx	h
	mov	m, e		; [sret+6] = result[6]
	inx	h
	mov	m, d		; [sret+7] = result[7]

	; Deallocate all stack locals: result(8) + mcand(8) + sign(2) = 18 bytes
	lxi	h, 18
	dad	sp
	sphl

	ret
	.size	__mulsi32, .-__mulsi32


; ===================================================================
; int32_t __mulsi32_shr16(int32_t a, int32_t b)
;   [SP+2..5]  = a  (4 bytes, little-endian, signed)
;   [SP+6..9]  = b  (4 bytes, little-endian, signed)
;   Returns (int32_t)( ((int64_t)a * (int64_t)b) >> 16 )
;   i.e. bytes [2..5] of the 64-bit product.
;   Result in BC:DE (C=byte0, B=byte1, E=byte2, D=byte3).
;
; This is Q15.16 fixed-point multiply.
;
; Optimisation: we need bytes 2-5 of the 8-byte product.
; We must track carry from bytes 0-1 into byte 2.
; We can skip accumulating into bytes 6-7.
; So we accumulate a 6-byte partial result (bytes 0-5) and
; return bytes 2-5.
;
; Strategy: same sign-handling as __mulsi32, then unsigned
; 32x32 shift-and-add with 6-byte result and 6-byte multiplicand.
; ===================================================================
	.section .text.__mulsi32_shr16, "ax", @progbits
	.globl	__mulsi32_shr16
	.type	__mulsi32_shr16,@function
__mulsi32_shr16:
	; Compute result sign: XOR sign bytes of a and b
#ifdef UNDOC
	ldsi	5
	ldax	d		; a[3]
#else
	lxi	h, 5
	dad	sp
	mov	a, m		; a[3]
#endif
	lxi	h, 9
	dad	sp
	xra	m		; XOR with b[3]
	push	psw		; save sign flag

	; --- Make a unsigned ---
	; a at [SP+4..7] (sign push +2)
#ifdef UNDOC
	ldsi	7
	ldax	d
#else
	lxi	h, 7
	dad	sp		; -> a[3]
	mov	a, m
#endif
	ora	a
	jp	.Lms32s16_a_pos

	lxi	h, 4
	dad	sp		; -> a[0]
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
.Lms32s16_a_pos:

	; --- Make b unsigned ---
	; b at [SP+8..11] (sign push +2)
#ifdef UNDOC
	ldsi	11
	ldax	d
#else
	lxi	h, 11
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lms32s16_b_pos

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
.Lms32s16_b_pos:

	; --- Allocate multiplicand (8 bytes: |b| zero-extended to 64) ---
	; Only 6 bytes would suffice but 8 keeps alignment simple.
	lxi	h, 0
	push	h		; mcand[6..7] = 0
	push	h		; mcand[4..5] = 0

	; Load |b| from [SP+12..15] (sign +2, mcand +4)
	lxi	h, 12
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d		; mcand[2..3]
	push	b		; mcand[0..1]

	; --- Allocate 8-byte result, zeroed ---
	lxi	h, 0
	push	h		; result[6..7] = 0  (padding)
	push	h		; result[4..5] = 0
	push	h		; result[2..3] = 0
	push	h		; result[0..1] = 0

	; Stack layout:
	;   [SP+ 0.. 1] = result[0..1]
	;   [SP+ 2.. 3] = result[2..3]
	;   [SP+ 4.. 5] = result[4..5]
	;   [SP+ 6.. 7] = result[6..7]  (padding)
	;   [SP+ 8.. 9] = mcand[0..1]
	;   [SP+10..11] = mcand[2..3]
	;   [SP+12..13] = mcand[4..5]
	;   [SP+14..15] = mcand[6..7]  (padding)
	;   [SP+16..17] = sign flag
	;   [SP+18..19] = return address
	;   [SP+20..23] = a (multiplier)
	;   [SP+24..27] = b (original)

.Lms32s16_loop:
	; Check if multiplier (a) == 0
	lxi	h, 20
	dad	sp
	mov	a, m
	inx	h
	ora	m
	inx	h
	ora	m
	inx	h
	ora	m
	jz	.Lms32s16_mul_done

	; Test bit 0 of multiplier
	lxi	h, 20
	dad	sp
	mov	a, m
	ani	1
	jz	.Lms32s16_no_add

	; --- result[0..5] += mcand[0..5] (6-byte add) ---
	; Strategy: load mcand[0..3] into C,B,E,D, add to result[0..3].
	; Then save carry, load mcand[4..5], add to result[4..5].
	; Must avoid DAD SP between carry-producing ADD/ADC and
	; carry-consuming ADC because DAD modifies the carry flag.

	lxi	h, 8
	dad	sp
	mov	c, m		; mcand[0]
	inx	h
	mov	b, m		; mcand[1]
	inx	h
	mov	e, m		; mcand[2]
	inx	h
	mov	d, m		; mcand[3]
	inx	h
	; HL now points to mcand[4]
	; Save mcand[4..5] BEFORE the add chain.
	push	h		; save pointer to mcand[4] (+2 shift)

	lxi	h, 2		; result[0] at SP+0+2 (push shifted)
	dad	sp
	mov	a, m
	add	c
	mov	m, a		; result[0]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[1]
	inx	h
	mov	a, m
	adc	e
	mov	m, a		; result[2]
	inx	h
	mov	a, m
	adc	d
	mov	m, a		; result[3]
	; HL now at result[3], carry is valid
	inx	h		; HL = result[4]
	; Save HL (result pointer) in DE
	mov	d, h
	mov	e, l		; DE = &result[4]

	; Load mcand[4..5] from saved pointer
	; (can't use DAD SP -- would destroy carry)
	pop	h		; HL = &mcand[4], carry is still valid
	mov	c, m		; mcand[4]
	inx	h
	mov	b, m		; mcand[5]
	; Note: MOV and INX don't affect carry

	; Continue add chain: result[4..5] += mcand[4..5] + carry
	xchg			; HL = &result[4], DE = junk
	mov	a, m
	adc	c
	mov	m, a		; result[4]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[5]

.Lms32s16_no_add:
	; Multiplier >>= 1
	lxi	h, 23
	dad	sp		; -> a[3]
	mov	a, m
	ora	a
	rar
	mov	m, a
	dcx	h
	mov	a, m
	rar
	mov	m, a
	dcx	h
	mov	a, m
	rar
	mov	m, a
	dcx	h
	mov	a, m
	rar
	mov	m, a

	; Multiplicand <<= 1 (6 bytes: mcand[0..5])
	lxi	h, 8
	dad	sp
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
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a

	jmp	.Lms32s16_loop

.Lms32s16_mul_done:
	; Load result bytes 0-5 for sign handling.
	; We need bytes 2-5 of the final (possibly negated) result.
	; For correct negation, load all 6 bytes.
	lxi	h, 0
	dad	sp
	mov	c, m		; result[0]
	inx	h
	mov	b, m		; result[1]
	; We need 6 bytes but only have 7 registers (A,B,C,D,E,H,L).
	; Store bytes 0-1 in BC, load bytes 2-5 separately after sign check.

	; Actually, for negation we need to negate in sequence with carry.
	; Easiest: negate in-place on stack, then extract bytes 2-5.

	; Deallocate mcand (8 bytes) + result padding
	; Actually let's just check sign and work with stack data.

	; Load sign flag from [SP+16..17]
	lxi	h, 16
	dad	sp
	mov	a, m		; sign byte
	ora	a
	jp	.Lms32s16_pos

	; Negate 6-byte result in-place on stack [SP+0..5]
	lxi	h, 0
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

.Lms32s16_pos:
	; Extract bytes 2-5 into return registers BC:DE
	lxi	h, 2
	dad	sp
	mov	c, m		; byte 2 -> C (byte 0 of return)
	inx	h
	mov	b, m		; byte 3 -> B (byte 1 of return)
	inx	h
	mov	e, m		; byte 4 -> E (byte 2 of return)
	inx	h
	mov	d, m		; byte 5 -> D (byte 3 of return)

	; Deallocate: result(8) + mcand(8) + sign(2) = 18 bytes
	lxi	h, 18
	dad	sp
	sphl

	ret
	.size	__mulsi32_shr16, .-__mulsi32_shr16


; ===================================================================
; int32_t __mulsi32_hi32(int32_t a, int32_t b)
;   [SP+2..5]  = a  (4 bytes, little-endian, signed)
;   [SP+6..9]  = b  (4 bytes, little-endian, signed)
;   Returns upper 32 bits of signed 64-bit product: bits [63:32].
;   Result in BC:DE (C=byte0, B=byte1, E=byte2, D=byte3).
;
; We need bytes 4-7 of the full 8-byte product.
; Must compute the full 64-bit product for carry accuracy.
; Uses same approach as __mulsi32 but returns bytes 4-7 only.
; ===================================================================
	.section .text.__mulsi32_hi32, "ax", @progbits
	.globl	__mulsi32_hi32
	.type	__mulsi32_hi32,@function
__mulsi32_hi32:
	; Compute result sign
#ifdef UNDOC
	ldsi	5
	ldax	d		; a[3]
#else
	lxi	h, 5
	dad	sp
	mov	a, m		; a[3]
#endif
	lxi	h, 9
	dad	sp
	xra	m		; XOR with b[3]
	push	psw		; save sign flag

	; --- Make a unsigned ---
#ifdef UNDOC
	ldsi	7
	ldax	d
#else
	lxi	h, 7
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lms32h_a_pos

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
.Lms32h_a_pos:

	; --- Make b unsigned ---
#ifdef UNDOC
	ldsi	11
	ldax	d
#else
	lxi	h, 11
	dad	sp
	mov	a, m
#endif
	ora	a
	jp	.Lms32h_b_pos

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
.Lms32h_b_pos:

	; --- Allocate multiplicand (8 bytes) ---
	lxi	h, 0
	push	h		; mcand[6..7] = 0
	push	h		; mcand[4..5] = 0

	lxi	h, 12
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d		; mcand[2..3]
	push	b		; mcand[0..1]

	; --- Allocate 8-byte result ---
	lxi	h, 0
	push	h		; result[6..7] = 0
	push	h		; result[4..5] = 0
	push	h		; result[2..3] = 0
	push	h		; result[0..1] = 0

	; Stack layout (same as __mulsi32):
	;   [SP+ 0.. 7] = result[0..7]
	;   [SP+ 8..15] = mcand[0..7]
	;   [SP+16..17] = sign flag
	;   [SP+18..19] = return address
	;   [SP+20..23] = a (multiplier)
	;   [SP+24..27] = b

.Lms32h_loop:
	lxi	h, 20
	dad	sp
	mov	a, m
	inx	h
	ora	m
	inx	h
	ora	m
	inx	h
	ora	m
	jz	.Lms32h_mul_done

	lxi	h, 20
	dad	sp
	mov	a, m
	ani	1
	jz	.Lms32h_no_add

	; --- result += mcand (full 8-byte add) ---
	; Must avoid DAD SP between carry-producing ADD/ADC and
	; carry-consuming ADC because DAD modifies the carry flag.
	lxi	h, 8
	dad	sp
	mov	c, m		; mcand[0]
	inx	h
	mov	b, m		; mcand[1]
	inx	h
	mov	e, m		; mcand[2]
	inx	h
	mov	d, m		; mcand[3]
	inx	h
	; HL now points to mcand[4]
	push	h		; save pointer to mcand[4] (+2 shift)

	lxi	h, 2		; result[0] at SP+0+2
	dad	sp
	mov	a, m
	add	c
	mov	m, a		; result[0]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[1]
	inx	h
	mov	a, m
	adc	e
	mov	m, a		; result[2]
	inx	h
	mov	a, m
	adc	d
	mov	m, a		; result[3]
	; HL at result[3], carry is valid
	inx	h		; HL = result[4]
	mov	d, h
	mov	e, l		; DE = &result[4]

	; Load mcand[4..7] from saved pointer
	pop	h		; HL = &mcand[4] (carry preserved)
	mov	c, m		; mcand[4]
	inx	h
	mov	b, m		; mcand[5]
	; Need mcand[6..7] too -- save in temporary stack slots
	inx	h
	push	h		; save &mcand[6] for later (+2)

	; Continue add chain: result[4..5] += mcand[4..5] + carry
	xchg			; HL = &result[4]
	mov	a, m
	adc	c
	mov	m, a		; result[4]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[5]
	; HL at result[5], carry still valid
	inx	h		; HL = result[6]
	mov	d, h
	mov	e, l		; DE = &result[6]

	; Load mcand[6..7]
	pop	h		; HL = &mcand[6] (carry preserved)
	mov	c, m		; mcand[6]
	inx	h
	mov	b, m		; mcand[7]

	xchg			; HL = &result[6]
	mov	a, m
	adc	c
	mov	m, a		; result[6]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[7]

.Lms32h_no_add:
	; Multiplier >>= 1
	lxi	h, 23
	dad	sp
	mov	a, m
	ora	a
	rar
	mov	m, a
	dcx	h
	mov	a, m
	rar
	mov	m, a
	dcx	h
	mov	a, m
	rar
	mov	m, a
	dcx	h
	mov	a, m
	rar
	mov	m, a

	; Multiplicand <<= 1 (8 bytes)
	lxi	h, 8
	dad	sp
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
	inx	h
	mov	a, m
	adc	a
	mov	m, a

	jmp	.Lms32h_loop

.Lms32h_mul_done:
	; Check sign and negate full 8-byte result if needed
	lxi	h, 16
	dad	sp
	mov	a, m
	ora	a
	jp	.Lms32h_pos

	; Negate 8-byte result in-place
	lxi	h, 0
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

.Lms32h_pos:
	; Extract bytes 4-7 into return registers BC:DE
	lxi	h, 4
	dad	sp
	mov	c, m		; result[4] -> C (byte 0 of return)
	inx	h
	mov	b, m		; result[5] -> B
	inx	h
	mov	e, m		; result[6] -> E
	inx	h
	mov	d, m		; result[7] -> D

	; Deallocate: result(8) + mcand(8) + sign(2) = 18 bytes
	lxi	h, 18
	dad	sp
	sphl

	ret
	.size	__mulsi32_hi32, .-__mulsi32_hi32


; ===================================================================
; uint16_t __mului8(uint8_t a, uint8_t b)
;   [SP+2] = a (1 byte, unsigned)
;   [SP+3] = b (1 byte, unsigned)
;   Returns unsigned 16-bit product in BC (B = high, C = low).
;
; This is the unsigned widening 8x8->16 multiply.
; Simpler than __mulsi8 because no sign handling is needed.
;
; Registers during multiply core:
;   D  = multiplier  (a, shifts right, 8-bit)
;   E  = multiplicand low byte (b, shifts left into B:E)
;   HL = 16-bit result accumulator
;   B  = multiplicand high byte (starts 0, grows as E shifts out)
; ===================================================================
	.section .text.__mului8, "ax", @progbits
	.globl	__mului8
	.type	__mului8,@function
__mului8:
	; Load a into D (multiplier)
	lxi	h, 2
	dad	sp
	mov	d, m

	; Load b into E (multiplicand)
	lxi	h, 3
	dad	sp
	mov	e, m

	; B = multiplicand high byte (starts 0)
	; HL = result accumulator (starts 0)
	mvi	b, 0
	lxi	h, 0

.Lmu8_loop:
	; Early exit when multiplier is exhausted
	mov	a, d
	ora	a
	jz	.Lmu8_done

	; Test LSB of multiplier
	ani	1
	jz	.Lmu8_no_add

	; result (HL) += multiplicand (B:E as 16-bit)
	mov	a, l
	add	e
	mov	l, a
	mov	a, h
	adc	b
	mov	h, a

.Lmu8_no_add:
	; multiplicand <<= 1  (B:E <<= 1)
	mov	a, e
	add	a		; E <<= 1
	mov	e, a
	mov	a, b
	adc	a		; B = B*2 + carry
	mov	b, a

	; multiplier >>= 1
	mov	a, d
	ora	a		; clear carry
	rar
	mov	d, a

	jmp	.Lmu8_loop

.Lmu8_done:
	; Move result from HL to BC
	mov	b, h
	mov	c, l
	ret
	.size	__mului8, .-__mului8


; ===================================================================
; uint32_t __mului16(uint16_t a, uint16_t b)
;   [SP+2..3] = a  (first arg, 16-bit unsigned)
;   [SP+4..5] = b  (second arg, 16-bit unsigned)
;   Returns unsigned 32-bit product in BC:DE
;   (C=byte0 LSB, B=byte1, E=byte2, D=byte3 MSB).
;
; This is the unsigned widening 16x16->32 multiply.
; Simpler than __mulsi16 because no sign handling is needed.
;
; Optimised: result lives in BC:DE (register-resident).
; Multiplier at top of stack, accessed via XTHL.
; ===================================================================
	.section .text.__mului16, "ax", @progbits
	.globl	__mului16
	.type	__mului16,@function
__mului16:
	; Load a into DE (multiplier)
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
#endif

	; Load b into BC (multiplicand)
	lxi	h, 4
	dad	sp
	mov	c, m
	inx	h
	mov	b, m

	; Allocate multiplicand on stack (4 bytes, zero-extended)
	lxi	h, 0
	push	h		; mcand[2..3] = 0
	push	b		; mcand[0..1] = b
	; Push multiplier to top of stack (for XTHL access)
	push	d		; [SP+0..1] = multiplier

	; Stack layout (offsets from current SP):
	;   [SP+0..1]  = multiplier (accessed via XTHL)
	;   [SP+2..5]  = multiplicand (b, zero-extended to 32-bit)
	;   [SP+6..7]  = return address
	;   [SP+8..9]  = a (original arg)
	;   [SP+10..11] = b (original arg)

	; Result lives in BC:DE -- register-resident.
	lxi	b, 0		; result bytes 0-1 (C=byte0, B=byte1)
	lxi	d, 0		; result bytes 2-3 (E=byte2, D=byte3)

.Lmu16_loop:
	; --- Load multiplier into HL, check if zero ---
	xthl			; HL = multiplier, [SP] = junk (old HL)
	mov	a, h
	ora	l
	jz	.Lmu16_mul_done

	; --- Shift multiplier right by 1, capture old bit0 in carry ---
	mov	a, h
	ora	a		; clear carry
	rar
	mov	h, a
	mov	a, l
	rar
	mov	l, a
	; carry = old bit 0 of multiplier

	; --- Save shifted multiplier back to stack ---
	xthl			; [SP] = shifted multiplier, HL = junk
	; (XTHL does not affect flags -- carry is preserved)

	; --- Branch: skip add if old bit 0 was 0 ---
	jnc	.Lmu16_no_add

	; --- result (BC:DE) += multiplicand (on stack) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]

	mov	a, c
	add	m		; C += mcand[0]
	mov	c, a

	inx	h		; -> mcand[1]
	mov	a, b
	adc	m		; B += mcand[1] + carry
	mov	b, a

	inx	h		; -> mcand[2]
	mov	a, e
	adc	m		; E += mcand[2] + carry
	mov	e, a

	inx	h		; -> mcand[3]
	mov	a, d
	adc	m		; D += mcand[3] + carry
	mov	d, a

.Lmu16_no_add:
	; --- Shift multiplicand left by 1 (4 bytes on stack) ---
	lxi	h, 2
	dad	sp		; HL -> mcand[0]
	mov	a, m
	add	a		; mcand[0] <<= 1
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

	jmp	.Lmu16_loop

.Lmu16_mul_done:
	; HL = 0 (multiplier was zero). Result is in BC:DE.
	; Deallocate multiplier + multiplicand (6 bytes)
	lxi	h, 6
	dad	sp
	sphl

	ret
	.size	__mului16, .-__mului16


; ===================================================================
; uint64_t __mului32(uint32_t a, uint32_t b)
;   Called via sret convention for i64 return:
;   [SP+2..3] = sret pointer (where to store 8-byte result)
;   [SP+4..7] = a  (4 bytes, little-endian, unsigned)
;   [SP+8..11] = b  (4 bytes, little-endian, unsigned)
;   Returns: void (writes 8 bytes to sret pointer)
;
; This is the unsigned widening 32x32->64 multiply.
; Simpler than __mulsi32 because no sign handling is needed.
;
; Stack frame for the unsigned core:
;   - 8 bytes for the 64-bit result (on stack, zeroed)
;   - multiplier = a (in-place on caller's stack, shifts right)
;   - multiplicand = b (8 bytes on our stack, zero-extended to 64 bits,
;     shifts left each iteration)
; ===================================================================
	.section .text.__mului32, "ax", @progbits
	.globl	__mului32
	.type	__mului32,@function
__mului32:
	; --- Allocate multiplicand (8 bytes: b zero-extended to 64 bits) ---
	; mcand[0..3] = b, mcand[4..7] = 0
	lxi	h, 0
	push	h		; mcand[6..7] = 0
	push	h		; mcand[4..5] = 0

	; Load b from [SP+12..15] (mcand +4 from original +8)
	lxi	h, 12
	dad	sp		; -> b[0]
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	push	d		; mcand[2..3] = b[2..3]
	push	b		; mcand[0..1] = b[0..1]

	; --- Allocate 8-byte result, zeroed ---
	lxi	h, 0
	push	h		; result[6..7] = 0
	push	h		; result[4..5] = 0
	push	h		; result[2..3] = 0
	push	h		; result[0..1] = 0

	; Stack layout (offsets from current SP):
	;   [SP+ 0.. 1] = result[0..1]
	;   [SP+ 2.. 3] = result[2..3]
	;   [SP+ 4.. 5] = result[4..5]
	;   [SP+ 6.. 7] = result[6..7]
	;   [SP+ 8.. 9] = mcand[0..1]
	;   [SP+10..11] = mcand[2..3]
	;   [SP+12..13] = mcand[4..5]
	;   [SP+14..15] = mcand[6..7]
	;   [SP+16..17] = return address
	;   [SP+18..19] = sret pointer
	;   [SP+20..23] = a (multiplier, shifts right)
	;   [SP+24..27] = b (original)

.Lmu32_loop:
	; --- Check if multiplier (a) == 0 ---
	lxi	h, 20
	dad	sp
	mov	a, m		; a[0]
	inx	h
	ora	m		; | a[1]
	inx	h
	ora	m		; | a[2]
	inx	h
	ora	m		; | a[3]
	jz	.Lmu32_mul_done

	; --- Test bit 0 of multiplier ---
	lxi	h, 20
	dad	sp
	mov	a, m
	ani	1
	jz	.Lmu32_no_add

	; --- result += multiplicand (8-byte add) ---
	; Load mcand[0..3] into registers.
	; IMPORTANT: DAD SP destroys carry flag, so we save the mcand[4]
	; pointer via push h / pop h (which does NOT affect flags).
	lxi	h, 8
	dad	sp		; -> mcand[0]
	mov	c, m		; mcand[0]
	inx	h
	mov	b, m		; mcand[1]
	inx	h
	mov	e, m		; mcand[2]
	inx	h
	mov	d, m		; mcand[3]
	inx	h		; -> mcand[4]
	push	h		; save pointer to mcand[4] (SP shifts +2)

	; Add to result[0..3]
	lxi	h, 2
	dad	sp		; -> result[0] (shifted +2 for push)
	mov	a, m
	add	c
	mov	m, a		; result[0] += mcand[0]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[1] += mcand[1] + carry
	inx	h
	mov	a, m
	adc	e
	mov	m, a		; result[2] += mcand[2] + carry
	inx	h
	mov	a, m
	adc	d
	mov	m, a		; result[3] += mcand[3] + carry

	; Carry from result[3] is live. Now we need mcand[4..7].
	inx	h		; -> result[4]
	mov	d, h
	mov	e, l		; DE = &result[4]

	pop	h		; HL = &mcand[4] (carry flag preserved!)
	mov	c, m		; mcand[4]
	inx	h
	mov	b, m		; mcand[5]
	inx	h
	push	h		; save pointer to mcand[6] (SP shifts +2)

	xchg			; HL = &result[4], DE saved
	mov	a, m
	adc	c
	mov	m, a		; result[4] += mcand[4] + carry
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[5] += mcand[5] + carry

	; Carry from result[5] is live. Need mcand[6..7].
	inx	h		; -> result[6]
	mov	d, h
	mov	e, l		; DE = &result[6]

	pop	h		; HL = &mcand[6] (carry flag preserved!)
	mov	c, m		; mcand[6]
	inx	h
	mov	b, m		; mcand[7]

	xchg			; HL = &result[6]
	mov	a, m
	adc	c
	mov	m, a		; result[6] += mcand[6] + carry
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[7] += mcand[7] + carry

.Lmu32_no_add:
	; --- Shift multiplier (a) right by 1 ---
	lxi	h, 23
	dad	sp		; -> a[3]
	mov	a, m
	ora	a		; clear carry
	rar
	mov	m, a
	dcx	h		; -> a[2]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[1]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[0]
	mov	a, m
	rar
	mov	m, a

	; --- Shift multiplicand (mcand) left by 1 (8 bytes) ---
	lxi	h, 8
	dad	sp		; -> mcand[0]
	mov	a, m
	add	a		; mcand[0] <<= 1
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[1]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[2]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[3]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[4]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[5]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[6]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; mcand[7]

	jmp	.Lmu32_loop

.Lmu32_mul_done:
	; Read sret pointer into DE
	lxi	h, 18
	dad	sp
	mov	e, m		; sret_lo
	inx	h
	mov	d, m		; sret_hi

	; Write result[0..3] to sret pointer
	lxi	h, 0
	dad	sp

	; Byte 0
	mov	a, m
	stax	d		; [sret+0] = result[0]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+1] = result[1]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+2] = result[2]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+3] = result[3]
	inx	h
	inx	d

	; DE now points to sret+4. HL points to result[4].
	; Copy bytes 4-7 using memory moves.
	push	d		; save sret+4 on stack (SP+2 shift)

	; Load result[4..7] into registers
	lxi	h, 6		; result[4] at SP+4+2=6
	dad	sp
	mov	c, m		; result[4]
	inx	h
	mov	b, m		; result[5]
	inx	h
	mov	e, m		; result[6]
	inx	h
	mov	d, m		; result[7]

	; Retrieve sret+4 into HL
	pop	h		; HL = sret+4 (SP-2 shift, back to normal)

	mov	m, c		; [sret+4] = result[4]
	inx	h
	mov	m, b		; [sret+5] = result[5]
	inx	h
	mov	m, e		; [sret+6] = result[6]
	inx	h
	mov	m, d		; [sret+7] = result[7]

	; Deallocate all stack locals: result(8) + mcand(8) = 16 bytes
	lxi	h, 16
	dad	sp
	sphl

	ret
	.size	__mului32, .-__mului32


; ===================================================================
; int64_t __muldi3(int64_t a, int64_t b)
;   Called via sret convention for i64 return:
;   [SP+2..3]  = sret pointer (where to store 8-byte result)
;   [SP+4..11] = a  (8 bytes, little-endian, multiplier)
;   [SP+12..19] = b (8 bytes, little-endian, multiplicand)
;   Returns: void (writes 8 bytes to sret pointer)
;
; This is the same-width i64 multiply: only the low 64 bits of the
; 128-bit product are needed (truncated multiply).
;
; Algorithm: LSB-first shift-and-add with early termination.
; The multiplier (a) shifts right each iteration.
; The multiplicand (b) shifts left each iteration.
; Both are modified in-place on the caller's stack.
;
; Stack frame:
;   8 bytes for result (on stack, zeroed)
;   multiplier = a (in-place on caller's stack, shifts right)
;   multiplicand = b (in-place on caller's stack, shifts left)
;
; Since we only need 64 bits of product, the multiplicand only needs
; 8 bytes of tracking (upper bits shifted out are irrelevant).
; ===================================================================
	.section .text.__muldi3, "ax", @progbits
	.globl	__muldi3
	.type	__muldi3,@function
__muldi3:
	; --- Allocate 8-byte result, zeroed ---
	lxi	h, 0
	push	h		; result[6..7] = 0
	push	h		; result[4..5] = 0
	push	h		; result[2..3] = 0
	push	h		; result[0..1] = 0

	; Stack layout (offsets from current SP):
	;   [SP+ 0.. 1] = result[0..1]
	;   [SP+ 2.. 3] = result[2..3]
	;   [SP+ 4.. 5] = result[4..5]
	;   [SP+ 6.. 7] = result[6..7]
	;   [SP+ 8.. 9] = return address
	;   [SP+10..11] = sret pointer
	;   [SP+12..19] = a (multiplier, shifts right)
	;   [SP+20..27] = b (multiplicand, shifts left)

.Lmd3_loop:
	; --- Check if multiplier (a) == 0 ---
	lxi	h, 12
	dad	sp
	mov	a, m		; a[0]
	inx	h
	ora	m		; | a[1]
	inx	h
	ora	m		; | a[2]
	inx	h
	ora	m		; | a[3]
	inx	h
	ora	m		; | a[4]
	inx	h
	ora	m		; | a[5]
	inx	h
	ora	m		; | a[6]
	inx	h
	ora	m		; | a[7]
	jz	.Lmd3_mul_done

	; --- Test bit 0 of multiplier ---
#ifdef UNDOC
	ldsi	12
	ldax	d
#else
	lxi	h, 12
	dad	sp
	mov	a, m
#endif
	ani	1
	jz	.Lmd3_no_add

	; --- result += multiplicand (8-byte add) ---
	; Load b[0..3] into registers.
	; IMPORTANT: DAD SP destroys carry flag, so we save the b[4]
	; pointer via push h / pop h (which does NOT affect flags).
	lxi	h, 20
	dad	sp		; -> b[0]
	mov	c, m		; b[0]
	inx	h
	mov	b, m		; b[1]
	inx	h
	mov	e, m		; b[2]
	inx	h
	mov	d, m		; b[3]
	inx	h		; -> b[4]
	push	h		; save pointer to b[4] (SP shifts +2)

	; Add to result[0..3]
	lxi	h, 2
	dad	sp		; -> result[0] (shifted +2 for push)
	mov	a, m
	add	c
	mov	m, a		; result[0] += b[0]
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[1] += b[1] + carry
	inx	h
	mov	a, m
	adc	e
	mov	m, a		; result[2] += b[2] + carry
	inx	h
	mov	a, m
	adc	d
	mov	m, a		; result[3] += b[3] + carry

	; Carry from result[3] is live. Now we need b[4..7].
	inx	h		; -> result[4]
	mov	d, h
	mov	e, l		; DE = &result[4]

	pop	h		; HL = &b[4] (carry flag preserved!)
	mov	c, m		; b[4]
	inx	h
	mov	b, m		; b[5]
	inx	h
	push	h		; save pointer to b[6] (SP shifts +2)

	xchg			; HL = &result[4], DE saved
	mov	a, m
	adc	c
	mov	m, a		; result[4] += b[4] + carry
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[5] += b[5] + carry

	; Carry from result[5] is live. Need b[6..7].
	inx	h		; -> result[6]
	mov	d, h
	mov	e, l		; DE = &result[6]

	pop	h		; HL = &b[6] (carry flag preserved!)
	mov	c, m		; b[6]
	inx	h
	mov	b, m		; b[7]

	xchg			; HL = &result[6]
	mov	a, m
	adc	c
	mov	m, a		; result[6] += b[6] + carry
	inx	h
	mov	a, m
	adc	b
	mov	m, a		; result[7] += b[7] + carry

.Lmd3_no_add:
	; --- Shift multiplier (a) right by 1 ---
	lxi	h, 19
	dad	sp		; -> a[7]
	mov	a, m
	ora	a		; clear carry
	rar
	mov	m, a
	dcx	h		; -> a[6]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[5]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[4]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[3]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[2]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[1]
	mov	a, m
	rar
	mov	m, a
	dcx	h		; -> a[0]
	mov	a, m
	rar
	mov	m, a

	; --- Shift multiplicand (b) left by 1 (8 bytes) ---
	lxi	h, 20
	dad	sp		; -> b[0]
	mov	a, m
	add	a		; b[0] <<= 1
	mov	m, a
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[1]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[2]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[3]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[4]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[5]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[6]
	inx	h
	mov	a, m
	adc	a
	mov	m, a		; b[7]

	jmp	.Lmd3_loop

.Lmd3_mul_done:
	; Read sret pointer into DE
	lxi	h, 10
	dad	sp
	mov	e, m		; sret_lo
	inx	h
	mov	d, m		; sret_hi

	; Write result[0..3] to sret pointer
	lxi	h, 0
	dad	sp

	mov	a, m
	stax	d		; [sret+0] = result[0]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+1] = result[1]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+2] = result[2]
	inx	h
	inx	d

	mov	a, m
	stax	d		; [sret+3] = result[3]
	inx	h
	inx	d

	; DE now points to sret+4. HL points to result[4].
	push	d		; save sret+4 on stack (SP+2 shift)

	; Load result[4..7] into registers
	lxi	h, 6		; result[4] at SP+4+2=6
	dad	sp
	mov	c, m		; result[4]
	inx	h
	mov	b, m		; result[5]
	inx	h
	mov	e, m		; result[6]
	inx	h
	mov	d, m		; result[7]

	; Retrieve sret+4 into HL
	pop	h		; HL = sret+4 (SP-2 shift, back to normal)

	mov	m, c		; [sret+4] = result[4]
	inx	h
	mov	m, b		; [sret+5] = result[5]
	inx	h
	mov	m, e		; [sret+6] = result[6]
	inx	h
	mov	m, d		; [sret+7] = result[7]

	; Deallocate all stack locals: result(8) = 8 bytes
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__muldi3, .-__muldi3
