; ==================================================================================
; Nixie Tube Clock
; smbaker@smbaker.com
; ==================================================================================

N_PORT  equ     0E0H
DB      equ     1       ; data bit mask
CB      equ     2       ; clock bit mask
LB      equ     4       ; latch bit mask

LF	equ	0AH
CR	equ	0DH

VARS    equ     8000h

HOUR    equ     VARS
MIN     equ     HOUR+1
SEC     equ     MIN+1
GPSST   equ     SEC+1

        .ORG    290h

START: 	call	ILPRNT
	db	'Scotts Nixie Tube Clock',CR,LF+80h

        ; fill in the time with some defaults
        LD      A, 12
        LD      (HOUR),A
        LD      A, 34
        LD      (MIN), A
        LD      A, 56
        LD      (SEC), A

        ; set baud rate to 9600 baud for GPS module
        LD      A, 9
        RST     28h

        CALL    DISP

RUNLOOP:
        CALL    CHCHAR

        LD      A, (GPSST)
        CP      A, 14          ; state 14 is when the parser has the whole time
        JR      NZ, NOTCPLT

        call    ILPRNT
        db      'Time:', ' '+80h
        LD      A, (HOUR)
        CALL OUTHXA
        LD      A, (MIN)
        CALL OUTHXA
        LD      A, (SEC)
        CALL OUTHXA
        call    ILPRNT
        db      CR, LF+80h

        CALL    DISP
NOTCPLT:
        JP      RUNLOOP

HANG:   jp      HANG

; =============================================================================
; GPS input
; =============================================================================

CHCHAR: LD      A, 1
        RST     18h           ; get port B status
        JR      NZ, GOTCHAR
        RET

GOTCHAR:
        LD      A, 1          ; get character from port 1
        RST     10h
        LD      C, A          ; into C

        LD      A, (GPSST)    ; load GPS parse state into A

        CP      1
        JR      NZ, NOT1

        LD      A,C
        CP      '$'
        JP      NZ, BAD
        LD      A, 2          ; set state 2
        LD      (GPSST), A
        JP      GOOD

NOT1:   CP      2
        JR      NZ, NOT2

        LD      A,C
        CP      'G'
        JP      NZ, BAD
;        RST     08h
        LD      A, 3          ; set state 3
        LD      (GPSST), A
        JP      GOOD

NOT2:   CP      3
        JR      NZ, NOT3

        LD      A,C
        CP      'P'
        JP      NZ, BAD
;        RST     08h
        LD      A, 4          ; set state 4
        LD      (GPSST), A
        JP      GOOD

NOT3:   CP      4
        JR      NZ, NOT4

        LD      A,C
        CP      'G'
        JP      NZ, BAD
;        RST     08h
        LD      A, 5          ; set state 5
        LD      (GPSST), A
        JP      GOOD

NOT4:   CP      5
        JR      NZ, NOT5

        LD      A,C
        CP      'G'
        JP      NZ, BAD
;        RST     08h
        LD      A, 6          ; set state 6
        LD      (GPSST), A
        JP      GOOD

NOT5:   CP      6
        JR      NZ, NOT6

        LD      A,C
        CP      'A'
        JP      NZ, BAD
;        RST     08h
        LD      A, 7          ; set state 7
        LD      (GPSST), A
        JP      GOOD

NOT6:   CP      7
        JR      NZ, NOT7

        LD      A,C
        CP      ','
        JP      NZ, BAD
;        RST     08h
        LD      A, 8          ; set state 8
        LD      (GPSST), A
        JP      GOOD

        ; hours

NOT7:   CP      8
        JR      NZ, NOT8

        LD      A,C
;        RST     08h
        SUB     48
        CALL    MUL10
        LD      (HOUR), A

        LD      A, 9          ; set state 9
        LD      (GPSST), A
        JP      GOOD

NOT8:   CP      9
        JR      NZ, NOT9

        LD      A,C
;        RST     08h
        SUB     48
        LD      C,A
        LD      A, (HOUR)
        ADD     C
        LD      (HOUR), A

        LD      A, 10          ; set state 10
        LD      (GPSST), A
        JP      GOOD

        ; minutes

NOT9:   CP      10
        JR      NZ, NOT10

        LD      A,C
;        RST     08h
        SUB     48
        CALL    MUL10
        LD      (MIN), A

        LD      A, 11          ; set state 11
        LD      (GPSST), A
        JP      GOOD

NOT10:  CP      11
        JR      NZ, NOT11

        LD      A,C
;        RST     08h
        SUB     48
        LD      C, A
        LD      A, (MIN)
        ADD     C
        LD      (MIN), A

        LD      A, 12          ; set state 12
        LD      (GPSST), A
        JP      GOOD

        ; seconds

NOT11:  CP      12
        JR      NZ, NOT12

        LD      A,C
;        RST     08h
        SUB     48
        CALL    MUL10
        LD      (SEC), A

        LD      A, 13          ; set state 13
        LD      (GPSST), A
        JP      GOOD

NOT12:  CP      13
        JR      NZ, NOT13

        LD      A,C
;        RST     08h
        SUB     48
        LD      C, A
        LD      A, (SEC)
        ADD     C
        LD      (SEC), A

        LD      A, 14          ; set state 14
        LD      (GPSST), A
        JP      GOOD

NOT13:
BAD:    LD      A, 1
        LD      (GPSST), A
GOOD:
        RET



; =============================================================================
; Nixie Display Routines
; =============================================================================

DISP:   PUSH    AF
        PUSH    BC
        PUSH    DE
        LD      D, 10         ; divide by 10

        LD      A, (MIN)
        LD      C, A
        CALL    DIV
        LD      A, C
        CALL    SDIG          ; ten min

        LD      A, 15
        CALL    SDIG          ; blank digit

        LD      A, 0
        CALL    SDIG          ; skip dp & lights

        LD      A, 0
        CALL    SDIG          ; skip dp & lights

        LD      A, (HOUR)

        ADD     17            ; add 17 hours for PDT

        ; convert 24hr to 12hr
        CP      13
        JR      C, NOSUB
        SUB     12
NOSUB:

        ; convert 24hr to 12hr
        CP      13
        JR      C, NOSUB2
        SUB     12
NOSUB2:

        LD      C, A
        CALL    DIV
        CALL    SDIG          ; one hour
        LD      A, C
        CALL    SDIG          ; ten hour

        LD      A, (SEC)
        LD      C, A
        CALL    DIV
        CALL    SDIG          ; one second
        LD      A, C
        CALL    SDIG          ; ten second

        LD      A, 0
        CALL    SDIG          ; skip dp and lights

        LD      A, 0
        CALL    SDIG          ; skip dp and lights

        LD      A, 15
        CALL    SDIG          ; blank digit

        LD      A, (MIN)
        LD      C, A
        CALL    DIV
        CALL    SDIG          ; one minute

        CALL    LATCH

        POP     DE
        POP     BC
        POP     AF
        RET

LATCH:  PUSH    AF
        LD      A, LB
        OUT     N_PORT, A
        LD      A, 0
        OUT     N_PORT, A
        POP     AF
        RET

SBIT:   PUSH AF
        AND     A, DB
        OUT     N_PORT, A     ; output data bit with clock low
        OR      A, CB
        OUT     N_PORT, A     ; output data bit with clock high
        AND     A, DB
        OUT     N_PORT, A     ; output data bit with clock low
        POP     AF
        RET

SDIG:   PUSH    AF
        PUSH    BC
        LD      C, 4          ; shift four bits

DIGLP:  PUSH    AF
        RRC     A             ; move the fourth bit...
        RRC     A
        RRC     A             ; ...to the first position
        CALL    SBIT
        POP     AF

        RLC     A             ; shift one bit left

        DEC     C
        JR      NZ, DIGLP          ; repeat for the remaining bits

        POP     BC
        POP     AF

        RET

; ============================================================================
; 8-bit division http://z80-heaven.wikidot.com/math#toc24
; Inputs:
;     C is the numerator
;     D is the denominator
; Outputs:
;     A is the remainder
;     B is 0
;     C is the result of C/D
;     D,E,H,L are not changed
;
; ============================================================================

DIV:    LD      B,8
        XOR     A
        SLA     C
        RLA
        CP      D
        JR      C, $+4
        INC     C
        SUB     D
        DJNZ    $-8
        RET

; =============================================================================
MUL10:  PUSH    BC
        LD      C, A
        ADD     A,C
        ADD     A,C
        ADD     A,C
        ADD     A,C
        ADD     A,C
        ADD     A,C
        ADD     A,C
        ADD     A,C
        ADD     A,C
        POP     BC
        RET

; =============================================================================
; Inline Print Subroutine
; =============================================================================

ILPRNT:	ex      (SP),HL		;Save hl, get msg addr
	push	BC

IPLOOP:	ld      A, (HL)
	and	7Fh		;strip end marker
	rst     08              ;print character
	ld      A, (HL)		;end?
	inc	HL		;Next byte
	or	a		;msb set?
	jp	P, IPLOOP   	;Do all bytes of msg

	pop	BC
	ex      (SP),HL		;Restore hl,
	;; ..get return address
	ret

OUTHXA: PUSH BC
        LD C, A
        CALL OUTHX
        POP BC
        RET

OUTHX:  PUSH    AF
        LD      A, C
        RRA             ;ROTATE
        RRA             ; FOUR
        RRA             ; BITS TO
        RRA             ; RIGHT
        CALL    HEX1    ;UPPER CHAR
        LD     A,C     ;LOWER CHAR
        CALL    HEX1
        POP     AF
        ret

HEX1:   push    BC
        push    AF
        AND     0FH     ;TAKE 4 BITS
        ADD     A,90H
        DAA             ;DAA TRICK
        ADC     A, 40H
        DAA
        RST     08H
        pop     AF
        pop     BC
        ret