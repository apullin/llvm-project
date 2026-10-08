; Hand-optimised i8085 64-bit arithmetic routines.
;
; Calling convention (sret for i64 return):
;   [SP+2..3]  = sret pointer (where to store 8-byte result)
;   [SP+4..11] = a (int64_t, 8 bytes, little-endian)
;   [SP+12..19] = b (int64_t, 8 bytes, little-endian)
;
; Key 8085 flag-safety notes:
;   INX/DCX rp: do NOT affect any flags (carry-safe)
;   MOV r,r / MOV r,M / MOV M,r: do NOT affect flags
;   STAX / LDAX: do NOT affect flags
;   PUSH/POP rp (B/D/H): do NOT affect flags
;   PUSH/POP PSW: saves/restores A + all flags (including carry)
;   MVI r,imm: does NOT affect flags
;   DAD rp: DESTROYS carry flag (sets it to DAD result carry)
;   XCHG: does NOT affect flags


; ===================================================================
; __adddi3: i64 add  --  optimised two-pass approach
;   void __adddi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..19] = b (8 bytes, little-endian)
;
; Performs result = a + b using an 8-byte carry chain.
;
; Strategy: two 4-byte passes. Each pass pre-loads one operand's
; bytes into registers (B,C + stack staging for 2 more bytes), then
; walks the other operand with INX H (flag-safe). Result bytes
; are written to sret via STAX D (flag-safe) as we go.
;
; Uses only 5 LXI+DAD SP total (vs 17 in the naive per-byte
; approach) and zero PUSH/POP PSW for carry preservation.
; Carry is bridged between passes via the ADI 0xFF trick.
; ===================================================================
	.section .text.__adddi3, "ax", @progbits
	.globl	__adddi3
	.type	__adddi3,@function
__adddi3:
	; ---- Stage b[0..3] into B,C + stack ----
	lxi	h, 12
	dad	sp		; HL = &b[0]           [DAD #1]
	mov	c, m		; C = b[0]
	inx	h
	mov	b, m		; B = b[1]
	inx	h
	mov	a, m		; A = b[2]
	inx	h
	mov	h, m		; H = b[3]
	mov	l, a		; L = b[2]
	push	h		; push b[3](H), b[2](L) for mid-chain POP  [SP -= 2]

	; ---- Load sret pointer into DE ----
#ifdef UNDOC
	ldsi	4
	lhlx
	mov	e, l
	mov	d, h		; DE = sret pointer
#else
	lxi	h, 4		; sret at original SP+2, now SP+2+2 = 4
	dad	sp		;                       [DAD #2]
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer
#endif

	; ---- Point HL to a[0] ----
	lxi	h, 6		; a[0] at original SP+4, now SP+4+2 = 6
	dad	sp		; HL = &a[0]           [DAD #3]

	; ========================================================
	; Carry chain: bytes 0-3 (completely unbroken)
	; All instructions below are flag-safe until noted.
	; ========================================================
	mov	a, m		; A = a[0]
	add	c		; a[0] + b[0]
	stax	d		; sret[0] = result
	inx	d		; DE -> sret+1
	inx	h		; HL -> a[1]

	mov	a, m		; A = a[1]
	adc	b		; a[1] + b[1] + carry
	stax	d		; sret[1]
	inx	d		; DE -> sret+2
	inx	h		; HL -> a[2]

	; POP B does NOT affect flags -- carry is preserved!
	pop	b		; C = b[2], B = b[3]   [SP back to original]

	mov	a, m		; A = a[2]
	adc	c		; a[2] + b[2] + carry
	stax	d		; sret[2]
	inx	d		; DE -> sret+3
	inx	h		; HL -> a[3]

	mov	a, m		; A = a[3]
	adc	b		; a[3] + b[3] + carry
	stax	d		; sret[3]
	inx	d		; DE -> sret+4

	; ========================================================
	; Bridge: capture carry as a byte value (0 or 1)
	; MVI does NOT affect flags on 8085.
	; ========================================================
	mvi	a, 0		; A = 0 (flags untouched)
	adc	a		; A = 0 + 0 + carry = carry_byte (0 or 1)
	mov	c, a		; C = carry_byte

	; ---- Stage b[4..7] into B,C + stack ----
	; Push carry_byte first (in BC) so we can reuse B,C for b values.
	push	b		; save carry_byte(C) on stack   [SP -= 2]

	lxi	h, 18		; b[4] at original SP+16, now SP+16+2 = 18
	dad	sp		; HL = &b[4]                    [DAD #4]
	mov	c, m		; C = b[4]
	inx	h
	mov	b, m		; B = b[5]
	inx	h
	mov	a, m		; A = b[6]
	inx	h
	mov	h, m		; H = b[7]
	mov	l, a		; L = b[6]
	push	h		; push b[7](H), b[6](L)        [SP -= 4 total]

	; ---- Recover carry_byte and navigate to a[4] ----
	; Stack layout (SP = original - 4):
	;   [SP+0] = b[6](L), [SP+1] = b[7](H)   -- b67 push
	;   [SP+2] = carry_byte(C), [SP+3] = 0(B) -- carry push
	;   [SP+4..5] = return addr
	;   [SP+6..7] = sret arg
	;   [SP+8..15] = a[0..7]
	;   [SP+16..23] = b[0..7]

	lxi	h, 2		; carry_byte is at SP+2
	dad	sp		;                               [DAD #5]
	mov	a, m		; A = carry_byte (0 or 1)

	; Convert carry_byte back to carry flag:
	; ADI 0xFF: if A=0 -> 0+0xFF=0xFF, carry=0
	;           if A=1 -> 1+0xFF=0x00, carry=1
	adi	0xFF		; carry flag = (carry_byte was 1)

	; Navigate to a[4] using INX H (flag-safe, preserves carry).
	; HL is at SP+2. a[4] is at SP+12. Delta = 10.
	inx	h		; SP+3
	inx	h		; SP+4
	inx	h		; SP+5
	inx	h		; SP+6
	inx	h		; SP+7
	inx	h		; SP+8
	inx	h		; SP+9
	inx	h		; SP+10
	inx	h		; SP+11
	inx	h		; SP+12 = &a[4]

	; ========================================================
	; Carry chain: bytes 4-7 (completely unbroken)
	; Carry flag restored from carry_byte above.
	; ========================================================
	mov	a, m		; A = a[4]
	adc	c		; a[4] + b[4] + carry
	stax	d		; sret[4]
	inx	d		; DE -> sret+5
	inx	h		; HL -> a[5]

	mov	a, m		; A = a[5]
	adc	b		; a[5] + b[5] + carry
	stax	d		; sret[5]
	inx	d		; DE -> sret+6
	inx	h		; HL -> a[6]

	; POP B for b[6..7] -- flag-safe!
	pop	b		; C = b[6], B = b[7]   [SP = original - 2]

	mov	a, m		; A = a[6]
	adc	c		; a[6] + b[6] + carry
	stax	d		; sret[6]
	inx	d		; DE -> sret+7
	inx	h		; HL -> a[7]

	mov	a, m		; A = a[7]
	adc	b		; a[7] + b[7] + carry
	stax	d		; sret[7]

	; ---- Clean up stack ----
	; The carry_byte push is still on the stack. Pop to restore SP.
	pop	b		; discard carry push   [SP back to original]

	ret
	.size	__adddi3, .-__adddi3


; ===================================================================
; __subdi3: i64 subtract  --  optimised two-pass approach
;   void __subdi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..19] = b (8 bytes, little-endian)
;
; Performs result = a - b using an 8-byte borrow chain.
; Same structure as __adddi3 but with SUB/SBB instead of ADD/ADC.
; ===================================================================
	.section .text.__subdi3, "ax", @progbits
	.globl	__subdi3
	.type	__subdi3,@function
__subdi3:
	; ---- Stage b[0..3] ----
	lxi	h, 12
	dad	sp		; HL = &b[0]
	mov	c, m		; C = b[0]
	inx	h
	mov	b, m		; B = b[1]
	inx	h
	mov	a, m		; A = b[2]
	inx	h
	mov	h, m		; H = b[3]
	mov	l, a		; L = b[2]
	push	h		; push b[3](H), b[2](L)  [SP -= 2]

	; ---- Load sret pointer into DE ----
#ifdef UNDOC
	ldsi	4
	lhlx
	mov	e, l
	mov	d, h		; DE = sret pointer
#else
	lxi	h, 4		; sret at SP+2+2 = 4
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer
#endif

	; ---- Point HL to a[0] ----
	lxi	h, 6		; a[0] at SP+4+2 = 6
	dad	sp		; HL = &a[0]

	; ========================================================
	; Borrow chain: bytes 0-3 (completely unbroken)
	; ========================================================
	mov	a, m		; A = a[0]
	sub	c		; a[0] - b[0]
	stax	d		; sret[0]
	inx	d
	inx	h

	mov	a, m		; A = a[1]
	sbb	b		; a[1] - b[1] - borrow
	stax	d		; sret[1]
	inx	d
	inx	h

	pop	b		; C = b[2], B = b[3]  (flag-safe)  [SP back to original]

	mov	a, m		; A = a[2]
	sbb	c		; a[2] - b[2] - borrow
	stax	d		; sret[2]
	inx	d
	inx	h

	mov	a, m		; A = a[3]
	sbb	b		; a[3] - b[3] - borrow
	stax	d		; sret[3]
	inx	d

	; ========================================================
	; Bridge: capture borrow as byte
	; ========================================================
	mvi	a, 0
	adc	a		; A = borrow_byte (0 or 1)
	mov	c, a

	; ---- Stage b[4..7] ----
	push	b		; save borrow_byte  [SP -= 2]

	lxi	h, 18		; b[4] at SP+16+2 = 18
	dad	sp
	mov	c, m		; C = b[4]
	inx	h
	mov	b, m		; B = b[5]
	inx	h
	mov	a, m		; A = b[6]
	inx	h
	mov	h, m		; H = b[7]
	mov	l, a		; L = b[6]
	push	h		; push b[7](H), b[6](L)  [SP -= 4 total]

	; ---- Recover borrow_byte and restore borrow flag ----
	lxi	h, 2
	dad	sp
	mov	a, m		; A = borrow_byte

	; Restore borrow flag: ADI 0xFF sets carry iff borrow_byte was 1
	adi	0xFF

	; Navigate to a[4] via INX H (flag-safe), 10 steps from SP+2
	inx	h		; SP+3
	inx	h		; SP+4
	inx	h		; SP+5
	inx	h		; SP+6
	inx	h		; SP+7
	inx	h		; SP+8
	inx	h		; SP+9
	inx	h		; SP+10
	inx	h		; SP+11
	inx	h		; SP+12 = &a[4]

	; ========================================================
	; Borrow chain: bytes 4-7 (completely unbroken)
	; ========================================================
	mov	a, m		; A = a[4]
	sbb	c		; a[4] - b[4] - borrow
	stax	d		; sret[4]
	inx	d
	inx	h

	mov	a, m		; A = a[5]
	sbb	b		; a[5] - b[5] - borrow
	stax	d		; sret[5]
	inx	d
	inx	h

	pop	b		; C = b[6], B = b[7]  (flag-safe)  [SP -= 2]

	mov	a, m		; A = a[6]
	sbb	c		; a[6] - b[6] - borrow
	stax	d		; sret[6]
	inx	d
	inx	h

	mov	a, m		; A = a[7]
	sbb	b		; a[7] - b[7] - borrow
	stax	d		; sret[7]

	; Clean up stack
	pop	b		; discard borrow push  [SP back to original]

	ret
	.size	__subdi3, .-__subdi3


; ===================================================================
; __anddi3: i64 bitwise AND
;   void __anddi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..19] = b (8 bytes, little-endian)
;
; Performs result = a & b, 8 bytes.
; ===================================================================
	.section .text.__anddi3, "ax", @progbits
	.globl	__anddi3
	.type	__anddi3,@function
__anddi3:
	; Load sret pointer into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h		; DE = sret pointer
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer
#endif

	; Process byte 0
	lxi	h, 4
	dad	sp
	mov	a, m		; a[0]
	lxi	h, 12
	dad	sp
	ana	m		; a[0] & b[0]
	stax	d
	inx	d

	; Byte 1
	lxi	h, 5
	dad	sp
	mov	a, m
	lxi	h, 13
	dad	sp
	ana	m
	stax	d
	inx	d

	; Byte 2
	lxi	h, 6
	dad	sp
	mov	a, m
	lxi	h, 14
	dad	sp
	ana	m
	stax	d
	inx	d

	; Byte 3
	lxi	h, 7
	dad	sp
	mov	a, m
	lxi	h, 15
	dad	sp
	ana	m
	stax	d
	inx	d

	; Byte 4
	lxi	h, 8
	dad	sp
	mov	a, m
	lxi	h, 16
	dad	sp
	ana	m
	stax	d
	inx	d

	; Byte 5
	lxi	h, 9
	dad	sp
	mov	a, m
	lxi	h, 17
	dad	sp
	ana	m
	stax	d
	inx	d

	; Byte 6
	lxi	h, 10
	dad	sp
	mov	a, m
	lxi	h, 18
	dad	sp
	ana	m
	stax	d
	inx	d

	; Byte 7
	lxi	h, 11
	dad	sp
	mov	a, m
	lxi	h, 19
	dad	sp
	ana	m
	stax	d

	ret
	.size	__anddi3, .-__anddi3


; ===================================================================
; __ordi3: i64 bitwise OR
;   void __ordi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..19] = b (8 bytes, little-endian)
;
; Performs result = a | b, 8 bytes.
; ===================================================================
	.section .text.__ordi3, "ax", @progbits
	.globl	__ordi3
	.type	__ordi3,@function
__ordi3:
	; Load sret pointer into DE
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

	; Byte 0
	lxi	h, 4
	dad	sp
	mov	a, m
	lxi	h, 12
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 1
	lxi	h, 5
	dad	sp
	mov	a, m
	lxi	h, 13
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 2
	lxi	h, 6
	dad	sp
	mov	a, m
	lxi	h, 14
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 3
	lxi	h, 7
	dad	sp
	mov	a, m
	lxi	h, 15
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 4
	lxi	h, 8
	dad	sp
	mov	a, m
	lxi	h, 16
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 5
	lxi	h, 9
	dad	sp
	mov	a, m
	lxi	h, 17
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 6
	lxi	h, 10
	dad	sp
	mov	a, m
	lxi	h, 18
	dad	sp
	ora	m
	stax	d
	inx	d

	; Byte 7
	lxi	h, 11
	dad	sp
	mov	a, m
	lxi	h, 19
	dad	sp
	ora	m
	stax	d

	ret
	.size	__ordi3, .-__ordi3


; ===================================================================
; __xordi3: i64 bitwise XOR
;   void __xordi3(int64_t *result, int64_t a, int64_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..19] = b (8 bytes, little-endian)
;
; Performs result = a ^ b, 8 bytes.
; ===================================================================
	.section .text.__xordi3, "ax", @progbits
	.globl	__xordi3
	.type	__xordi3,@function
__xordi3:
	; Load sret pointer into DE
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

	; Byte 0
	lxi	h, 4
	dad	sp
	mov	a, m
	lxi	h, 12
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 1
	lxi	h, 5
	dad	sp
	mov	a, m
	lxi	h, 13
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 2
	lxi	h, 6
	dad	sp
	mov	a, m
	lxi	h, 14
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 3
	lxi	h, 7
	dad	sp
	mov	a, m
	lxi	h, 15
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 4
	lxi	h, 8
	dad	sp
	mov	a, m
	lxi	h, 16
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 5
	lxi	h, 9
	dad	sp
	mov	a, m
	lxi	h, 17
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 6
	lxi	h, 10
	dad	sp
	mov	a, m
	lxi	h, 18
	dad	sp
	xra	m
	stax	d
	inx	d

	; Byte 7
	lxi	h, 11
	dad	sp
	mov	a, m
	lxi	h, 19
	dad	sp
	xra	m
	stax	d

	ret
	.size	__xordi3, .-__xordi3


; ===================================================================
; __negdi2: i64 negate
;   void __negdi2(int64_t *result, int64_t a)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;
; Performs result = -a = ~a + 1.
; Complement all 8 bytes and add 1 with carry propagation.
; ===================================================================
	.section .text.__negdi2, "ax", @progbits
	.globl	__negdi2
	.type	__negdi2,@function
__negdi2:
	; Load sret pointer into DE
#ifdef UNDOC
	ldsi	2
	lhlx
	mov	e, l
	mov	d, h		; DE = sret pointer
#else
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer
#endif

	; Complement and add 1 with carry chain
	; Byte 0: ~a[0] + 1
	lxi	h, 4
	dad	sp
	mov	a, m
	cma
	adi	1
	stax	d
	inx	d

	; Byte 1: ~a[1] + carry
	lxi	h, 5
	dad	sp
	mov	a, m
	cma
	; Need to preserve carry from byte 0. But LXI/DAD destroyed it.
	; Must use a different approach.

	; Restart: read all bytes, negate, write.
	; Since DAD destroys carry, we cannot use a running carry chain
	; across DAD SP instructions. Instead:
	; Load all 8 bytes, negate in registers/stack, write out.

	; Re-load sret pointer (we already advanced DE)
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer

	; Load a[0..3] into registers
	lxi	h, 4
	dad	sp
	mov	c, m		; a[0]
	inx	h
	mov	b, m		; a[1]
	inx	h
	push	h		; save pointer to a[2]
	mov	a, m
	inx	h
	mov	h, m		; a[3]
	mov	l, a		; L = a[2], H = a[3]

	; Negate: ~val + 1
	; Byte 0
	mov	a, c
	cma
	adi	1
	stax	d		; [sret+0]
	inx	d

	; Byte 1
	mov	a, b
	cma
	aci	0		; + carry from byte 0
	stax	d		; [sret+1]
	inx	d

	; Byte 2
	mov	a, l
	cma
	aci	0
	stax	d		; [sret+2]
	inx	d

	; Byte 3
	mov	a, h
	cma
	aci	0
	stax	d		; [sret+3]
	inx	d

	; Save carry from byte 3 and DE (sret+4)
	; carry is live. CMA/ACI don't use DAD, so carry chain is intact.
	; But we need to load a[4..7]. DAD SP will destroy carry.
	; Save carry as 0/1 in a register.
	mvi	a, 0
	adc	a		; A = carry (0 or 1)
	mov	c, a		; C = carry from lower 4 bytes

	pop	h		; restore pointer to a[2], but we need a[4]
	; a[2] was at original SP+6. After pop, SP is same as after first push.
	; Actually we pushed h before the negate. Let's fix the pointer.
	; After pop h, HL = saved pointer to a[2] from before push.
	; a[4] is at a[2] + 2
	inx	h
	inx	h		; HL -> a[4]
	mov	b, m		; a[4]
	inx	h
	push	h		; save pointer to a[5]
	mov	a, m		; a[5]
	inx	h
	mov	l, m		; a[6]
	inx	h
	mov	h, m		; a[7]
	; Regs: B=a[4], A=a[5], L=a[6], H=a[7], C=carry, DE=sret+4

	; Negate byte 4: ~a[4] + carry_from_lower
	push	psw		; save A (a[5])
	mov	a, b
	cma
	add	c		; ~a[4] + carry (0 or 1)
	stax	d		; [sret+4]
	inx	d

	; Byte 5
	pop	psw		; restore a[5] (carry from ADD C is lost)
	; We need carry from byte 4. The ADD C above may have carried.
	; This approach is broken. Let me use a cleaner strategy.

	; ===== CLEAN RESTART for __negdi2 =====
	; Pop everything we messed up
	pop	h		; discard

	; Simple strategy: allocate 8 bytes on stack, copy input,
	; negate in-place with carry chain (no DAD SP interruption),
	; then copy to sret.

	; Re-read sret pointer
	lxi	h, 2
	dad	sp
	mov	e, m
	inx	h
	mov	d, m
	push	d		; save sret pointer (+2)

	; Copy input to stack (8 bytes)
	; a[0..3] at SP+4+2=6
	lxi	h, 6
	dad	sp
	mov	c, m		; a[0]
	inx	h
	mov	b, m		; a[1]
	inx	h
	mov	e, m		; a[2]
	inx	h
	mov	d, m		; a[3]
	push	d		; a[2..3] (+2)
	push	b		; a[0..1] (+2)

	; a[4..7] at SP+8+4=12 (shifted by sret push+2, a01 push+2, a23 push+2 = +6)
	lxi	h, 16		; a[4] at original SP+8 + 6 pushes... recalculate
	; Original stack: [SP+2..3]=sret, [SP+4..11]=a
	; After push sret (+2): a at SP+6..13
	; After push d (a23) (+2): a at SP+8..15
	; After push b (a01) (+2): a at SP+10..17
	; a[4] is at SP+10+4 = SP+14
	lxi	h, 14
	dad	sp
	mov	c, m		; a[4]
	inx	h
	mov	b, m		; a[5]
	inx	h
	mov	e, m		; a[6]
	inx	h
	mov	d, m		; a[7]

	; Now negate all 8 bytes with carry chain.
	; Bytes 0-1 on stack at [SP+0..1] (push b) and [SP+2..3] (push d)
	; Let me pop them back.
	; Stack: [SP+0..1]=a[0..1], [SP+2..3]=a[2..3], [SP+4..5]=sret
	; Regs: C=a[4], B=a[5], E=a[6], D=a[7]
	; Need to negate: a[0], a[1], a[2], a[3], a[4](C), a[5](B), a[6](E), a[7](D)

	; Pop a[0..1] and a[2..3] into HL and save
	; Actually let me re-think. We have limited registers.
	; The cleanest approach: negate the lower 4 bytes on stack,
	; then negate upper 4 bytes in registers, then write all to sret.

	; Pop a[0..1]
	pop	h		; H=a[1], L=a[0] -- wait, PUSH B pushes B at SP-1, C at SP-2
	; On 8085: PUSH rp pushes high byte first, then low byte.
	; Memory: [SP+0] = C (low), [SP+1] = B (high)
	; POP H: L = [SP+0] = C = a[0], H = [SP+1] = B = a[1]
	; So L = a[0], H = a[1]

	; Negate byte 0
	mov	a, l
	cma
	adi	1
	mov	l, a		; negated a[0]

	; Negate byte 1
	mov	a, h
	cma
	aci	0
	mov	h, a		; negated a[1]

	; Save HL (negated a[0..1]) and carry
	push	psw		; save carry (+2)
	push	h		; save negated a[0..1] (+2)

	; Pop a[2..3] from stack
	; a[2..3] was pushed as PUSH D (E at SP+0, D at SP+1)
	; After our 2 pushes, it's at SP+4..5
	; But we can't pop it directly since our pushes are on top.
	; Read from stack instead.
	lxi	h, 4
	dad	sp		; -> a[2] (original push d position shifted by our 2 pushes)
	; Wait, original stack had: [SP]=a01, [SP+2]=a23, [SP+4]=sret
	; After pop h (-2), push psw (+2), push h (+2): net +2
	; So a[2..3] now at original SP+2+2 = SP+4
	mov	l, m		; a[2]
	inx	h
	mov	h, m		; a[3]

	; Restore carry
	; PSW is at our pushed position. We pushed psw then h.
	; Stack: [SP+0..1]=negated a01, [SP+2..3]=PSW
	; But we need carry before negating bytes 2-3.
	; Let me redo this more carefully...

	; This is getting too complex. Let me use the same per-byte
	; approach as __adddi3 with PSW push/pop to preserve carry.
	; Pop everything and start fresh.
	pop	h		; discard
	pop	psw		; discard
	; Stack now has a[2..3] and sret on it. Pop those too.
	pop	h		; discard a[2..3]
	pop	d		; restore sret pointer
	; Stack is back to original.

	; ===== DEFINITIVE APPROACH for __negdi2 =====
	; Use the same PSW push/pop carry-preservation technique as __adddi3.
	; Write results directly to sret pointer as we go.
	; For each byte: read a[i], complement, add carry (or 1 for first), write to sret.

	; DE = sret pointer (just restored above)

	; Byte 0: ~a[0] + 1
	lxi	h, 4
	dad	sp
	mov	a, m
	cma
	adi	1
	stax	d
	inx	d
	push	psw		; save carry (+2)

	; Byte 1: ~a[1] + carry
	lxi	h, 7		; a[1] at SP+4+1+2(push)=7
	dad	sp
	mov	c, m		; C = a[1]
	pop	psw		; restore carry
	mov	a, c
	cma
	aci	0
	stax	d
	inx	d
	push	psw		; save carry (+2)

	; Byte 2
	lxi	h, 8		; a[2] at SP+4+2+2=8
	dad	sp
	mov	c, m
	pop	psw
	mov	a, c
	cma
	aci	0
	stax	d
	inx	d
	push	psw

	; Byte 3
	lxi	h, 9		; a[3] at SP+4+3+2=9
	dad	sp
	mov	c, m
	pop	psw
	mov	a, c
	cma
	aci	0
	stax	d
	inx	d
	push	psw

	; Byte 4
	lxi	h, 10		; a[4] at SP+4+4+2=10
	dad	sp
	mov	c, m
	pop	psw
	mov	a, c
	cma
	aci	0
	stax	d
	inx	d
	push	psw

	; Byte 5
	lxi	h, 11		; a[5] at SP+4+5+2=11
	dad	sp
	mov	c, m
	pop	psw
	mov	a, c
	cma
	aci	0
	stax	d
	inx	d
	push	psw

	; Byte 6
	lxi	h, 12		; a[6] at SP+4+6+2=12
	dad	sp
	mov	c, m
	pop	psw
	mov	a, c
	cma
	aci	0
	stax	d
	inx	d
	push	psw

	; Byte 7
	lxi	h, 13		; a[7] at SP+4+7+2=13
	dad	sp
	mov	c, m
	pop	psw
	mov	a, c
	cma
	aci	0
	stax	d

	ret
	.size	__negdi2, .-__negdi2


; ===================================================================
; __cmpdi2: signed 64-bit three-way compare
;   int __cmpdi2(int64_t a, int64_t b)
;   [SP+2..9]  = a (8 bytes, little-endian)
;   [SP+10..17] = b (8 bytes, little-endian)
;   Returns: 0 if a<b, 1 if a==b, 2 if a>b
;   Return in A (also in BC for i16/i32 compat: C=result, B=0)
;
; Algorithm: for signed comparison, check signs first.
; If signs differ, the negative number is smaller.
; If signs are the same, compare as unsigned from MSB to LSB.
; ===================================================================
	.section .text.__cmpdi2, "ax", @progbits
	.globl	__cmpdi2
	.type	__cmpdi2,@function
__cmpdi2:
	; Load sign bytes: a[7] at [SP+9], b[7] at [SP+17]
	lxi	h, 9
	dad	sp
	mov	b, m		; B = a[7] (sign byte of a)
	lxi	h, 17
	dad	sp
	mov	c, m		; C = b[7] (sign byte of b)

	; Check if signs differ
	mov	a, b
	xra	c
	jp	.Lcmpdi2_same_sign	; bit 7 clear -> same sign

	; Signs differ: negative < positive
	; If a is negative (b[7] bit 7 set), a < b -> return 0
	; If b is negative (a[7] bit 7 clear, b[7] bit 7 set), a > b -> return 2
	mov	a, b
	ora	a
	jm	.Lcmpdi2_ret0	; a is negative -> a < b
	jmp	.Lcmpdi2_ret2	; a is positive -> a > b

.Lcmpdi2_same_sign:
	; Same sign: compare unsigned from MSB (byte 7) to LSB (byte 0)
	; Compare byte 7
#ifdef UNDOC
	ldsi	9
	ldax	d		; a[7]
#else
	lxi	h, 9
	dad	sp
	mov	a, m		; a[7]
#endif
	lxi	h, 17
	dad	sp
	cmp	m		; compare a[7] vs b[7]
	jc	.Lcmpdi2_ret0	; a[7] < b[7] -> a < b
	jnz	.Lcmpdi2_ret2	; a[7] > b[7] -> a > b

	; Byte 6
#ifdef UNDOC
	ldsi	8
	ldax	d
#else
	lxi	h, 8
	dad	sp
	mov	a, m
#endif
	lxi	h, 16
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; Byte 5
#ifdef UNDOC
	ldsi	7
	ldax	d
#else
	lxi	h, 7
	dad	sp
	mov	a, m
#endif
	lxi	h, 15
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; Byte 4
#ifdef UNDOC
	ldsi	6
	ldax	d
#else
	lxi	h, 6
	dad	sp
	mov	a, m
#endif
	lxi	h, 14
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; Byte 3
#ifdef UNDOC
	ldsi	5
	ldax	d
#else
	lxi	h, 5
	dad	sp
	mov	a, m
#endif
	lxi	h, 13
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; Byte 2
#ifdef UNDOC
	ldsi	4
	ldax	d
#else
	lxi	h, 4
	dad	sp
	mov	a, m
#endif
	lxi	h, 12
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; Byte 1
#ifdef UNDOC
	ldsi	3
	ldax	d
#else
	lxi	h, 3
	dad	sp
	mov	a, m
#endif
	lxi	h, 11
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; Byte 0
#ifdef UNDOC
	ldsi	2
	ldax	d
#else
	lxi	h, 2
	dad	sp
	mov	a, m
#endif
	lxi	h, 10
	dad	sp
	cmp	m
	jc	.Lcmpdi2_ret0
	jnz	.Lcmpdi2_ret2

	; All bytes equal -> return 1
	mvi	a, 1
	mov	c, a
	mvi	b, 0
	ret

.Lcmpdi2_ret0:
	mvi	a, 0
	mov	c, a
	mvi	b, 0
	ret

.Lcmpdi2_ret2:
	mvi	a, 2
	mov	c, a
	mvi	b, 0
	ret
	.size	__cmpdi2, .-__cmpdi2


; ===================================================================
; __ucmpdi2: unsigned 64-bit three-way compare
;   int __ucmpdi2(uint64_t a, uint64_t b)
;   [SP+2..9]  = a (8 bytes, little-endian)
;   [SP+10..17] = b (8 bytes, little-endian)
;   Returns: 0 if a<b, 1 if a==b, 2 if a>b
;   Return in A (also in BC for i16/i32 compat: C=result, B=0)
;
; Algorithm: compare bytes from MSB (byte 7) to LSB (byte 0).
; ===================================================================
	.section .text.__ucmpdi2, "ax", @progbits
	.globl	__ucmpdi2
	.type	__ucmpdi2,@function
__ucmpdi2:
	; Compare byte 7 (MSB)
#ifdef UNDOC
	ldsi	9
	ldax	d		; a[7]
#else
	lxi	h, 9
	dad	sp
	mov	a, m		; a[7]
#endif
	lxi	h, 17
	dad	sp
	cmp	m		; compare a[7] vs b[7]
	jc	.Lucmpdi2_ret0	; a < b
	jnz	.Lucmpdi2_ret2	; a > b

	; Byte 6
#ifdef UNDOC
	ldsi	8
	ldax	d
#else
	lxi	h, 8
	dad	sp
	mov	a, m
#endif
	lxi	h, 16
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; Byte 5
#ifdef UNDOC
	ldsi	7
	ldax	d
#else
	lxi	h, 7
	dad	sp
	mov	a, m
#endif
	lxi	h, 15
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; Byte 4
#ifdef UNDOC
	ldsi	6
	ldax	d
#else
	lxi	h, 6
	dad	sp
	mov	a, m
#endif
	lxi	h, 14
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; Byte 3
#ifdef UNDOC
	ldsi	5
	ldax	d
#else
	lxi	h, 5
	dad	sp
	mov	a, m
#endif
	lxi	h, 13
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; Byte 2
#ifdef UNDOC
	ldsi	4
	ldax	d
#else
	lxi	h, 4
	dad	sp
	mov	a, m
#endif
	lxi	h, 12
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; Byte 1
#ifdef UNDOC
	ldsi	3
	ldax	d
#else
	lxi	h, 3
	dad	sp
	mov	a, m
#endif
	lxi	h, 11
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; Byte 0 (LSB)
#ifdef UNDOC
	ldsi	2
	ldax	d
#else
	lxi	h, 2
	dad	sp
	mov	a, m
#endif
	lxi	h, 10
	dad	sp
	cmp	m
	jc	.Lucmpdi2_ret0
	jnz	.Lucmpdi2_ret2

	; All bytes equal
	mvi	a, 1
	mov	c, a
	mvi	b, 0
	ret

.Lucmpdi2_ret0:
	mvi	a, 0
	mov	c, a
	mvi	b, 0
	ret

.Lucmpdi2_ret2:
	mvi	a, 2
	mov	c, a
	mvi	b, 0
	ret
	.size	__ucmpdi2, .-__ucmpdi2
