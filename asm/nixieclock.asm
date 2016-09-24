; ==================================================================================
; Nixie Tube Clock
; smbaker@smbaker.com
;
; Nixie - port E0h
; CTC - port 90h
; SIO - port 80h  (PORTA = console, PORTB = GPS at 9600 baud)
; SP0256A-AL2 - port 20h (optional)
; ==================================================================================

SPC_PORT equ    20H

N_PORT  equ     0E0H
DB      equ     1       ; data bit mask
CB      equ     2       ; clock bit mask
LB      equ     4       ; latch bit mask

LF	equ	0AH
CR	equ	0DH

TIMEZONE equ    -7      ; -7 = PDT

SPC_BUFSIZE     .EQU     8FH

; ==================================================================================
; Variables
;    Put them at 8100h because int_sio's buffers are around 8000h - 80FFh.
; ==================================================================================

VARS    equ     8100h   ; space for storing variables

HOUR    equ     VARS
MIN     equ     HOUR+1
SEC     equ     MIN+1
GPSST   equ     SEC+1

SPC_BUF     equ     GPSST+1
SPC_IN_PTR  equ     SPC_BUF+SPC_BUFSIZE
SPC_RD_PTR  equ     SPC_IN_PTR+2
SPC_USED    equ     SPC_RD_PTR+2
SPC_IN_MASK equ     SPC_IN_PTR&$FF

JUNK        equ     SPC_USED+2

; ==================================================================================
; Start of program
; ==================================================================================


        .ORG    290h

START: 	call	ILPRNT
	db	'Scotts Nixie Tube Clock',CR,LF+80h

        call    TALKER_INIT

        LD      A, 23
        CALL    SAYNUM

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

;        call    ILPRNT
;        db      'Time:', ' '+80h
;        LD      A, (HOUR)
;        CALL OUTHXA
;        LD      A, (MIN)
;        CALL OUTHXA
;        LD      A, (SEC)
;        CALL OUTHXA
;        call    ILPRNT
;        db      CR, LF+80h

        CALL    DISP
NOTCPLT:
        CALL    TALKER_POLL
        JP      RUNLOOP

HANG:   jp      HANG

SAYTIME:
        CALL    CONVHR
        CALL    SAYNUM

        LD      A, (MIN)
        CP      A, 0
        JR      NZ, NOTZERO
        CALL    SAYNUM
NOTZERO:

        CALL    CONVHR
        LD      A,B
        CP      0
        JR      NZ, SAYPM
        LD      HL, D_AM
        CALL    SAY_HL
        RET

SAYPM:
        LD      HL, D_PM
        CALL    SAY_HL
        RET


; =============================================================================
; GPS input
;   A simple state machine for $GPGGA sentences
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

        CALL    CONVHR

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

        ; read hours and convert from 24h to 12h format
        ; applies timezone setting
        ; returns 12hr hour in A, B=0 if AM or B=1 if PM
CONVHR: LD      A, (HOUR)
        LD      B, 0

        ADD     24
        ADD     TIMEZONE

        ; in case the timezone add caused us to wrap
        CP      25
        JR      C, NOSUB
        SUB     24
NOSUB:
        CP      12
        JR      C, NOTPM
        LD      B, 1
NOTPM:
        ; convert 24hr to 12hr
        CP      13
        JR      C, NOSUB2
        SUB     12
NOSUB2:
        RET

        ; latch the nixie display
LATCH:  PUSH    AF
        LD      A, LB
        OUT     N_PORT, A
        LD      A, 0
        OUT     N_PORT, A
        POP     AF
        RET

        ; shift a data bit onto the display
SBIT:   PUSH AF
        AND     A, DB
        OUT     N_PORT, A     ; output data bit with clock low
        OR      A, CB
        OUT     N_PORT, A     ; output data bit with clock high
        AND     A, DB
        OUT     N_PORT, A     ; output data bit with clock low
        POP     AF
        RET

        ; shift a 4-bit digit to the display
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
; Multiply by 10
; Inputs: A
; Outputs: A
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

ILPRNT: ex      (SP),HL		;Save hl, get msg addr
        push    AF
	push	BC

IPLOOP:	ld      A, (HL)
	and	7Fh		;strip end marker
	rst     08              ;print character
	ld      A, (HL)		;end?
	inc	HL		;Next byte
	or	a		;msb set?
	jp	P, IPLOOP   	;Do all bytes of msg

	pop	BC
        pop     AF
	ex      (SP),HL		;Restore hl,
	;; ..get return address
	ret

; =============================================================================
; Print hex value Subroutines
;    OUTHXA: print the 8-bit hex value in A
;    OUTHXHL: print the 16-bit hex value in HL
;    OUTHX: print the 8-bit hex value in C
; =============================================================================

OUTHXA: PUSH BC
        LD C, A
        CALL OUTHX
        POP BC
        RET

OUTHXHL: PUSH AF
        LD      A, H
        CALL    OUTHXA
        LD      A, L
        CALL    OUTHXA
        POP     AF
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

; ============================================================================
; talker
;    TALKER_INIT: initialize the talker
;    TALKER_PUT: put a phoneme into the buffer
;    TALKER_POLL: if a phoneme is in the buffer, and the sp0256 is ready, then
;                 say the phoneme.
;    SAYNUM: say a number from 0 to 59
;    SAY_HL: say a list of phonemes pointed to by HL
; ============================================================================

TALKER_INIT:
               ; initialize first serial port
               LD        HL, SPC_BUF
               LD        (SPC_IN_PTR),HL
               LD        (SPC_RD_PTR),HL
               XOR       A               ;0 to accumulator
               LD        (SPC_USED),A
               RET

TALKER_PUT:    PUSH     HL
               PUSH     AF
               LD       A,(SPC_USED)
               CP       SPC_BUFSIZE     ; If full then ignore
               JR       NZ,notFull
               POP      AF
               POP      HL
               RET

notFull:       LD       HL,(SPC_IN_PTR)
               INC      HL
               LD       A,L             ; Only need to check low byte becasuse buffer<256 bytes
               CP       SPC_IN_MASK
               JR       NZ, notWrap
               LD       HL, SPC_BUF
notWrap:       LD       (SPC_IN_PTR),HL
               POP      AF
               LD       (HL),A

;               CALL     ILPRNT
;               DB       'W-'+80h
;               CALL     OUTHXHL
;               CALL     ILPRNT
;               DB       ':'+80h
;               CALL     OUTHXA
;               CALL     ILPRNT
;               DB       ' '+80h

               LD       A,(SPC_USED)
               INC      A
               LD       (SPC_USED),A
               POP      HL
               RET

TALKER_POLL:   IN       A, SPC_PORT
               AND      2
               CP       2
               JR       NZ, ready
               RET

ready:         LD       A,(SPC_USED)
               CP       0
               JR       NZ, notEmpty
               RET

notEmpty:      PUSH     HL
               LD       HL,(SPC_RD_PTR)
               INC      HL
               LD       A,L             ; Only need to check low byte becasuse buffer<256 bytes
               CP       SPC_IN_MASK
               JR       NZ, notRdWrap
               LD       HL, SPC_BUF
notRdWrap:     LD       (SPC_RD_PTR),HL
               LD       A,(SPC_USED)
               DEC      A
               LD       (SPC_USED),A
               LD       A,(HL)

;               CALL     ILPRNT
;               DB       'R-'+80h
;               CALL     OUTHXHL
;               CALL     ILPRNT
;               DB       ':'+80h
;               CALL     OUTHXA
;               CALL     ILPRNT
;               DB       ' '+80h

               POP      HL

               OUT      SPC_PORT, A

               RET

SAYNUM: CALL     SAYNUM1
        LD      A, 1
        CALL    TALKER_PUT

        CALL    ILPRNT
        DB      CR,LF+80h

        RET

SAYNUM1: CP      21
        JR      NC, GRT20

        CALL    SAYDIG

        RET

GRT20:  CP      30
        JR      NC, GRT29

        LD      HL, D_20
        CALL    SAY_HL
        SUB     A, 20
        CALL    SAYDIG

        RET

GRT29:  CP      30
        JR      NZ, NOT30

        LD      HL, D_30
        CALL    SAY_HL
        RET

NOT30:  CP      40
        JR      NC, GRT39

        LD      HL, D_30
        CALL    SAY_HL
        SUB     A, 30
        CALL    SAYDIG
        RET

GRT39:  CP      40
        JR      NZ, NOT40

        LD      HL, D_40
        CALL    SAY_HL
        RET

NOT40:  CP      50
        JR      NC, GRT49

        LD      HL, D_40
        CALL    SAY_HL
        SUB     A, 40
        CALL    SAYDIG
        RET

GRT49:  CP      50
        JR      NZ, NOT50

        LD      HL, D_50
        CALL    SAY_HL
        RET

NOT50:  CP      60
        JR      NC, GRT59

        LD      HL, D_50
        CALL    SAY_HL
        SUB     A, 50
        CALL    SAYDIG
        RET

GRT59:  RET

SAYDIG: LD      B, 0
        LD      C, A      ; BC = A*2
        LD      HL, P_0
        ADD     HL, BC    ; HL = P_0 + A
        ADD     HL, BC    ; HL = P_0 + A*2

;        CALL    ILPRNT
;        DB      ' '+80h

        ; is there an easier way to "LD HL, (HL)" ??
        LD      A, (HL)
        INC     HL
        LD      B, (HL)
        LD      H, B
        LD      L, A

        CALL    SAY_HL
        RET

SAY_HL: PUSH    AF
        PUSH    HL
SAY_LP: LD      A, (HL)
        CP      0
        JR      NZ, NOTDONE
        POP     HL
        POP     AF
        RET
NOTDONE:
        ;CALL    OUTHXA
        CALL    TALKER_PUT
        INC     HL
        JP      SAY_LP

D_0      DB 43, 60, 53, 0
D_1      DB 46, 15, 11, 0
D_2      DB 13, 31, 0
D_3      DB 29, 14, 19, 0
D_4      DB 40, 40, 58, 0
D_5      DB 40, 40, 6, 35, 0
D_6      DB 55, 55, 12, 12, 2, 41, 55, 0
D_7      DB 55, 55, 7, 7, 35, 12, 11, 0
D_8      DB 20, 2, 13, 0
D_9      DB 11, 24, 6, 11, 0
D_10     DB 13, 7, 7, 11, 0
D_11     DB 12, 45, 7, 7, 35, 12, 11, 0
D_12     DB 13, 48, 7, 7, 45, 35, 0
D_13     DB 29, 51, 1, 2, 13, 19, 11, 0
D_14     DB 40, 58, 1, 2, 13, 19, 11, 0
D_15     DB 40, 12, 40, 1, 2, 13, 19, 11, 0
D_16     DB 55, 55, 12, 2, 41, 55, 1, 2, 13, 19, 11, 0
D_17     DB 55, 55, 7, 35, 29, 11, 1, 2, 13, 19, 11, 0
D_18     DB 20, 1, 2, 13, 19, 11, 0
D_19     DB 11, 6, 11, 1, 2, 13, 19, 11, 0
D_20     DB 13, 48, 7, 7, 11, 1, 2, 13, 19, 0
D_30     DB 29, 52, 1, 2, 13, 19, 0
D_40     DB 40, 58, 2, 13, 19, 0
D_50     DB 40, 40, 12, 40, 40, 1, 2, 13, 19, 0
D_60     DB 55, 55, 12, 2, 41, 55, 1, 2, 13, 19, 0
D_70     DB 55, 55, 7, 35, 12, 11, 1, 2, 13, 19, 0
D_80     DB 20, 2, 13, 19, 0
D_90     DB 11, 6, 11, 2, 13, 19, 0
D_HUN    DB 57, 15, 15, 11, 1, 33, 39, 12, 12, 0, 21, 0
D_THO    DB 29, 24, 32, 43, 29, 0, 0, 11, 21, 0
D_MIL    DB 16, 12, 12, 45, 49, 15, 11, 0
D_AM     DB 20, 1, 7, 7, 16, 0
D_PM     DB 9, 19, 1, 7, 7, 16, 0

P_0      .WORD D_0
P_1      .WORD D_1
P_2      .WORD D_2
P_3      .WORD D_3
P_4      .WORD D_4
P_5      .WORD D_5
P_6      .WORD D_6
P_7      .WORD D_7
P_8      .WORD D_8
P_9      .WORD D_9
P_10     .WORD D_10
P_11     .WORD D_11
P_12     .WORD D_12
P_13     .WORD D_13
P_14     .WORD D_14
P_15     .WORD D_15
P_16     .WORD D_16
P_17     .WORD D_17
P_18     .WORD D_18
P_19     .WORD D_19
P_20     .WORD D_20
P_30     .WORD D_30
P_40     .WORD D_40
P_50     .WORD D_50
P_60     .WORD D_60
P_70     .WORD D_70
P_80     .WORD D_80
P_90     .WORD D_90
P_HUN    .WORD D_HUN
P_THO    .WORD D_THO
P_MIL    .WORD D_MIL
P_AM     .WORD D_AM
P_PM     .WORD D_PM
