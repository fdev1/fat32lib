
.equ PRIMARY_OSCILLATOR,				0x2
.equ PRIMARY_OSCILLATOR_WITH_PLL,		0x3

.global _clock_sw
.include "p33fxxxx.inc"

_clock_sw:

	push w0
	push w1
	push w2
	push w3

	;
	; Place the New Oscillator Selection (NOSC=0b011) in W0
	;
	mov.b #0x3, w0
	
	;
	; OSCCONH (high byte) Unlock Sequence
	;
	mov #OSCCONH, w1
	mov.b #0x78, w2
	mov.b #0x9A, w3
	mov.b w2, [w1] ; Write 0x78
	mov.b w3, [w1] ; Write 0x9A
	
	;
	; Set New Oscillator Selection
	;
	mov.b w0, [w1]
	
	;
	; Place 0x01 in W0 for setting clock switch enabled bit
	;
	mov.b #0x1, w0
	
	;
	; OSCCONL (low byte) Unlock Sequence
	;
	mov #OSCCONL, w1
	mov.b #0x46, w2
	mov.b #0x57, w3
	mov.b w2, [w1] ; Write 0x46
	mov.b w3, [w1] ; Write 0x9A
	
	;
	; begin clock switch by setting OSWEN bit
	;
	mov.b w0, [w1]
	
	;
	; wait until the clock switch has
	; completed.
	;
wait:
	btsc OSCCONL, #OSWEN
	bra wait
		
	pop w3
	pop w2
	pop w1
	pop w0
		
	return
