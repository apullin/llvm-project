; Hand-optimised i8085 64-bit shift routines.
;
; Replaces the compiler-rt C implementations and the inline DAG
; expansion that produces 1000+ instructions per i64 shift.
;
; Calling convention (sret for i64 return):
;   [SP+2..3]  = sret pointer (where to store 8-byte result)
;   [SP+4..11] = a (int64_t, 8 bytes, little-endian)
;   [SP+12..15] = b (int32_t, shift amount -- only low byte used)
;
; Algorithm: byte-shuffle for multiples of 8, then bit-level
;            rotate for the remaining 0-7 bits.


; ===================================================================
; __ashldi3: i64 left shift
;   void __ashldi3(int64_t *result, int64_t a, int32_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..15] = b (shift amount, only low 6 bits matter)
;
; For left shift, byte shuffle moves bytes toward higher addresses
; (toward MSB), and bit shift uses ADD/ADC carry chain.
; ===================================================================
	.section .text.__ashldi3, "ax", @progbits
	.globl	__ashldi3
	.type	__ashldi3,@function
__ashldi3:
	; Copy input value to stack working area (8 bytes)
	; We work in-place on a stack copy.
	lxi	h, 4
	dad	sp
	mov	c, m		; a[0]
	inx	h
	mov	b, m		; a[1]
	push	b		; [SP] = a[0..1]  (will be adjusted)

	inx	h
	mov	c, m		; a[2]
	inx	h
	mov	b, m		; a[3]
	inx	h
	mov	e, m		; a[4]
	inx	h
	mov	d, m		; a[5]
	inx	h
	push	h		; save pointer to a[6]
	mov	l, m
	lxi	h, 0
	dad	sp		; save current SP position
	push	h		; save SP pointer
	; Load a[7]
	; Actually this approach is getting complicated. Let me use
	; a simpler method: copy all 8 bytes to stack.
	pop	h		; restore
	pop	h		; restore saved pointer
	pop	b		; restore a[0..1]

	; Simpler approach: allocate 8 bytes, copy input there.
	; We'll use the local stack area as our working buffer.
	lxi	h, 10		; a[6] at SP+10
	dad	sp
	mov	a, m
	push	psw		; save a[6]
	lxi	h, 12		; a[7] at SP+11+1=12 (pushed +1... wait)
	; This is getting messy with the pushes. Let me restart with
	; a cleaner approach.
	pop	psw		; undo

	; Clean approach: allocate 8 bytes on stack, then copy input.
	; Push 8 bytes of space (will be overwritten)
	lxi	h, 0
	push	h		; work[6..7]
	push	h		; work[4..5]
	push	h		; work[2..3]
	push	h		; work[0..1]

	; Stack layout:
	;   [SP+ 0.. 7] = work area (our copy of the value)
	;   [SP+ 8.. 9] = return address
	;   [SP+10..11] = sret pointer
	;   [SP+12..19] = a (input)
	;   [SP+20..23] = b (shift amount)

	; Copy a[0..7] from input to work area
	lxi	h, 12
	dad	sp		; -> a[0]
	; Copy to work area byte by byte
	; Load a[0..3] into BCDE
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m

	; Store to work area
	lxi	h, 0
	dad	sp
	mov	m, c		; work[0]
	inx	h
	mov	m, b		; work[1]
	inx	h
	mov	m, e		; work[2]
	inx	h
	mov	m, d		; work[3]

	; Load a[4..7]
	lxi	h, 16
	dad	sp		; -> a[4]
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m

	; Store to work area
	lxi	h, 4
	dad	sp
	mov	m, c		; work[4]
	inx	h
	mov	m, b		; work[5]
	inx	h
	mov	m, e		; work[6]
	inx	h
	mov	m, d		; work[7]

	; Load shift amount (only low 6 bits matter for 64-bit shift)
	lxi	h, 20
	dad	sp
	mov	a, m		; low byte of shift amount
	ani	63		; mask to 0-63

	; Now perform the shift.
	; First: handle byte-level shifts (multiples of 8)

.Lashl64_byte_loop:
	cpi	8
	jc	.Lashl64_bit_shift	; amount < 8, do bit shifts

	; Byte shuffle left: move each byte up by 1 position
	; work[7] = work[6], work[6] = work[5], ..., work[1] = work[0], work[0] = 0
	push	psw		; save shift amount
	lxi	h, 0
	dad	sp		; -> (pushed psw on top) need +2 for work
	lxi	h, 2
	dad	sp		; -> work[0]

	; Read work[6] down to work[0], shift up
	lxi	h, 8		; work[6] at SP+2+6=8
	dad	sp
	mov	d, m		; work[6]
	dcx	h
	mov	e, m		; work[5]
	dcx	h
	mov	b, m		; work[4]
	dcx	h
	mov	c, m		; work[3]
	; Store shifted: work[7]=d, work[6]=e, work[5]=b, work[4]=c
	lxi	h, 9		; work[7] at SP+2+7=9
	dad	sp
	mov	m, d		; work[7] = old work[6]
	dcx	h
	mov	m, e		; work[6] = old work[5]
	dcx	h
	mov	m, b		; work[5] = old work[4]
	dcx	h
	mov	m, c		; work[4] = old work[3]

	; Now handle work[0..3] -> work[1..3], work[0]=0
	lxi	h, 4		; work[2] at SP+2+2=4
	dad	sp
	mov	d, m		; work[2]
	dcx	h
	mov	e, m		; work[1]
	dcx	h
	mov	c, m		; work[0]
	; Store: work[3]=d, work[2]=e, work[1]=c, work[0]=0
	lxi	h, 5		; work[3] at SP+2+3=5
	dad	sp
	mov	m, d		; work[3] = old work[2]
	dcx	h
	mov	m, e		; work[2] = old work[1]
	dcx	h
	mov	m, c		; work[1] = old work[0]
	dcx	h
	mvi	m, 0		; work[0] = 0

	pop	psw		; restore shift amount
	sui	8		; amount -= 8
	jmp	.Lashl64_byte_loop

.Lashl64_bit_shift:
	; Remaining amount is 0-7 in A
	ora	a
	jz	.Lashl64_done

	; Bit-by-bit left shift loop
	mov	e, a		; E = remaining count

.Lashl64_bit_loop:
	; Shift all 8 bytes left by 1 bit using ADD/ADC carry chain
	lxi	h, 0
	dad	sp
	mov	a, m
	add	a		; work[0] <<= 1
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[1]
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[2]
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[3]
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[4]
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[5]
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[6]
	mov	m, a
	inx	h
	mov	a, m
	adc	a		; work[7]
	mov	m, a

	dcr	e
	jnz	.Lashl64_bit_loop

.Lashl64_done:
	; Write result to sret pointer
	; sret pointer at [SP+10..11]
	lxi	h, 10
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer

	; Copy work[0..3] to [sret+0..3]
	lxi	h, 0
	dad	sp
	mov	a, m
	stax	d		; [sret+0]
	inx	h
	inx	d
	mov	a, m
	stax	d		; [sret+1]
	inx	h
	inx	d
	mov	a, m
	stax	d		; [sret+2]
	inx	h
	inx	d
	mov	a, m
	stax	d		; [sret+3]

	; Copy work[4..7] to [sret+4..7]
	; DE now at sret+4. Need to use HL for stores since STAX only works with BC/DE.
	inx	d		; DE = sret+4
	inx	h		; HL -> work[4]
	; Save sret+4 pointer, load work[4..7]
	push	d		; save sret+4
	mov	c, m		; work[4]
	inx	h
	mov	b, m		; work[5]
	inx	h
	mov	e, m		; work[6]
	inx	h
	mov	d, m		; work[7]
	pop	h		; HL = sret+4
	mov	m, c		; [sret+4]
	inx	h
	mov	m, b		; [sret+5]
	inx	h
	mov	m, e		; [sret+6]
	inx	h
	mov	m, d		; [sret+7]

	; Deallocate 8-byte work area
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__ashldi3, .-__ashldi3


; ===================================================================
; __lshrdi3: i64 logical right shift
;   void __lshrdi3(int64_t *result, int64_t a, int32_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..15] = b (shift amount)
;
; For right shift, byte shuffle moves bytes toward lower addresses
; (toward LSB), and bit shift uses carry chain from MSB to LSB.
; Zeros are shifted in at the MSB side.
; ===================================================================
	.section .text.__lshrdi3, "ax", @progbits
	.globl	__lshrdi3
	.type	__lshrdi3,@function
__lshrdi3:
	; Allocate 8-byte work area
	lxi	h, 0
	push	h
	push	h
	push	h
	push	h

	; Stack layout:
	;   [SP+ 0.. 7] = work area
	;   [SP+ 8.. 9] = return address
	;   [SP+10..11] = sret pointer
	;   [SP+12..19] = a
	;   [SP+20..23] = b

	; Copy a to work area
	lxi	h, 12
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	lxi	h, 0
	dad	sp
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	lxi	h, 16
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	lxi	h, 4
	dad	sp
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	; Load shift amount
	lxi	h, 20
	dad	sp
	mov	a, m
	ani	63

.Llshr64_byte_loop:
	cpi	8
	jc	.Llshr64_bit_shift

	; Byte shuffle right: work[0]=work[1], work[1]=work[2], ..., work[6]=work[7], work[7]=0
	push	psw
	lxi	h, 3
	dad	sp		; work[1] at SP+2+1=3
	mov	c, m		; work[1]
	inx	h
	mov	b, m		; work[2]
	inx	h
	mov	e, m		; work[3]
	inx	h
	mov	d, m		; work[4]
	; Store: work[0]=c, work[1]=b, work[2]=e, work[3]=d
	lxi	h, 2		; work[0] at SP+2+0=2
	dad	sp
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	; work[5..7] -> work[4..6], work[7]=0
	lxi	h, 7		; work[5] at SP+2+5=7
	dad	sp
	mov	c, m		; work[5]
	inx	h
	mov	b, m		; work[6]
	inx	h
	mov	e, m		; work[7]
	lxi	h, 6		; work[4] at SP+2+4=6
	dad	sp
	mov	m, c		; work[4] = old work[5]
	inx	h
	mov	m, b		; work[5] = old work[6]
	inx	h
	mov	m, e		; work[6] = old work[7]
	inx	h
	mvi	m, 0		; work[7] = 0

	pop	psw
	sui	8
	jmp	.Llshr64_byte_loop

.Llshr64_bit_shift:
	ora	a
	jz	.Llshr64_done

	mov	e, a		; E = remaining count

.Llshr64_bit_loop:
	; Shift all 8 bytes right by 1 bit, MSB first (carry propagates down)
	; Logical right shift: zero enters at MSB
	lxi	h, 7
	dad	sp		; -> work[7]
	mov	a, m
	ora	a		; clear carry (zero enters at top)
	rar			; work[7] >>= 1
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[6]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[5]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[4]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[3]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[2]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[1]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[0]
	mov	m, a

	dcr	e
	jnz	.Llshr64_bit_loop

.Llshr64_done:
	; Write result to sret pointer
	lxi	h, 10
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer

	lxi	h, 0
	dad	sp
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
	mov	a, m
	stax	d

	inx	d
	inx	h
	push	d
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	pop	h
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	; Deallocate work area
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__lshrdi3, .-__lshrdi3


; ===================================================================
; __ashrdi3: i64 arithmetic right shift
;   void __ashrdi3(int64_t *result, int64_t a, int32_t b)
;   [SP+2..3]  = sret pointer
;   [SP+4..11] = a (8 bytes, little-endian)
;   [SP+12..15] = b (shift amount)
;
; Same as logical right shift but sign-extends from MSB.
; Byte shuffle fills with 0x00 or 0xFF depending on sign.
; Bit shift uses arithmetic right shift on the MSB byte.
; ===================================================================
	.section .text.__ashrdi3, "ax", @progbits
	.globl	__ashrdi3
	.type	__ashrdi3,@function
__ashrdi3:
	; Allocate 8-byte work area
	lxi	h, 0
	push	h
	push	h
	push	h
	push	h

	; Stack layout:
	;   [SP+ 0.. 7] = work area
	;   [SP+ 8.. 9] = return address
	;   [SP+10..11] = sret pointer
	;   [SP+12..19] = a
	;   [SP+20..23] = b

	; Copy a to work area
	lxi	h, 12
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	lxi	h, 0
	dad	sp
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	lxi	h, 16
	dad	sp
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	lxi	h, 4
	dad	sp
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	; Load shift amount
	lxi	h, 20
	dad	sp
	mov	a, m
	ani	63

.Lashr64_byte_loop:
	cpi	8
	jc	.Lashr64_bit_shift

	; Byte shuffle right with sign extension:
	; work[0]=work[1], ..., work[6]=work[7], work[7]=sign_byte
	; Sign byte = 0xFF if work[7] bit 7 is set, else 0x00
	push	psw

	; Determine sign fill byte from current work[7]
	lxi	h, 9		; work[7] at SP+2+7=9
	dad	sp
	mov	a, m
	ral			; carry = sign bit
	sbb	a		; A = 0xFF if negative, 0x00 if positive
	mov	d, a		; D = sign fill byte

	; Shuffle right: work[0]=work[1], ..., work[6]=work[7], work[7]=sign
	; Read from low to high (safe: read work[i+1] before write work[i])

	; Batch 1: read work[1..4], write to work[0..3]
	lxi	h, 3		; work[1] at SP+2+1=3
	dad	sp
	mov	c, m		; work[1]
	inx	h
	mov	b, m		; work[2]
	inx	h
	mov	e, m		; work[3]
	inx	h
	push	psw		; save shift amount again (SP-=2, offsets shift)
	mov	a, m		; work[4] (now at SP+4+4=8, but HL already there)
	lxi	h, 4		; work[0] at SP'+2+0=4 (after extra push, +2)
	dad	sp
	mov	m, c		; work[0] = old work[1]
	inx	h
	mov	m, b		; work[1] = old work[2]
	inx	h
	mov	m, e		; work[2] = old work[3]
	inx	h
	mov	m, a		; work[3] = old work[4]
	pop	psw		; restore (SP+=2, back to original layout)

	; Batch 2: read work[5..7], write to work[4..6], work[7]=sign
	lxi	h, 7		; work[5] at SP+2+5=7
	dad	sp
	mov	c, m		; work[5]
	inx	h
	mov	b, m		; work[6]
	inx	h
	mov	e, m		; work[7]
	lxi	h, 6		; work[4] at SP+2+4=6
	dad	sp
	mov	m, c		; work[4] = old work[5]
	inx	h
	mov	m, b		; work[5] = old work[6]
	inx	h
	mov	m, e		; work[6] = old work[7]
	inx	h
	mov	m, d		; work[7] = sign fill byte

	pop	psw
	sui	8
	jmp	.Lashr64_byte_loop

.Lashr64_bit_shift:
	ora	a
	jz	.Lashr64_done

	mov	e, a		; E = remaining count

.Lashr64_bit_loop:
	; Arithmetic right shift by 1 bit:
	; MSB byte gets arithmetic shift (sign preserved),
	; all other bytes get rotate-right-through-carry.
	lxi	h, 7
	dad	sp		; -> work[7]
	mov	a, m
	; Arithmetic shift right: preserve sign bit
	; On 8085: to do ASR, we check bit 7, then RAR.
	; If bit 7 is set, set carry before RAR. If clear, clear carry.
	ora	a		; sets sign flag based on bit 7
	jp	.Lashr64_msb_clear
	stc			; set carry (sign bit = 1)
	jmp	.Lashr64_msb_shift
.Lashr64_msb_clear:
	ora	a		; clear carry (also clears from any prior)
.Lashr64_msb_shift:
	rar			; A >>= 1, old carry (= sign bit) enters bit 7
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[6]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[5]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[4]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[3]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[2]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[1]
	mov	m, a
	dcx	h
	mov	a, m
	rar			; work[0]
	mov	m, a

	dcr	e
	jnz	.Lashr64_bit_loop

.Lashr64_done:
	; Write result to sret pointer
	lxi	h, 10
	dad	sp
	mov	e, m
	inx	h
	mov	d, m		; DE = sret pointer

	lxi	h, 0
	dad	sp
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
	mov	a, m
	stax	d

	inx	d
	inx	h
	push	d
	mov	c, m
	inx	h
	mov	b, m
	inx	h
	mov	e, m
	inx	h
	mov	d, m
	pop	h
	mov	m, c
	inx	h
	mov	m, b
	inx	h
	mov	m, e
	inx	h
	mov	m, d

	; Deallocate work area
	lxi	h, 8
	dad	sp
	sphl

	ret
	.size	__ashrdi3, .-__ashrdi3
