;==================================================================================
; Floppy Tester
; http://www.smbaker.com/
;
; Portions based on original source of Grant Searle (http://searle.hostei.com/grant/index.html)
;
;==================================================================================

LF		.EQU	0AH		;line feed
FF		.EQU	0CH		;form feed
CR		.EQU	0DH		;carriage RETurn

;====================================================================================

		.ORG	8100H		; Format program origin.

INIT            LD        HL,$F000        ;  lets get lots of stack space...
                LD        SP,HL           ; Set up a temporary stack

		CALL	printInline
		.TEXT "CP/M Formatter for 1.44 floppy"
		.DB CR,LF,0

                CALL    FD_INIT

                LD      B, DOP_READID
                CALL    FD_DISPATCH

                CALL    printInline
                .TEXT   "Media ",0
                CALL    OUTHXA
                CALL    printInline
                .TEXT   CR, LF, 0

                ; point disk buffer to directory entries
                LD      HL, dirData
                LD      (DIOBUF), HL

; There are 512 directory entries per disk, 4 DIR entries per sector
; So 128 x 128 byte sectors are to be initialised
; The drive uses 512 byte sectors, so 32 x 512 byte sectors per disk
; require initialisation

;Drive 0 (A:) is slightly different due to reserved track, so DIR sector starts at 32
		LD	A,$20
		LD	(secNo),A

                ; start at block $20, which is Track 2 Sector 15
                ;    (track numbers start at 1, sectors start at 0)
                LD      HL, 2
                LD      (HSTTRK), HL
                LD      HL, $0E
                LD      (HSTSEC), HL

processSectorA:
                ; write the sector

                ; C = disk num
                ; HL = track
                ; DE = sec
                ; B = function

                LD      C, 0
                LD      HL, (HSTTRK)
                LD      DE, (HSTSEC)
                LD      B, DOP_WRITE

                call    PRINTOP

                call    FD_DISPATCH

                call    PRINTRES

                ; increment sector

                LD      DE, (HSTSEC)
                INC     DE
                LD      A, E
                CP      17
                JR      C, NOWRAP
                LD      DE, 0
                LD      HL, (HSTTRK)
                INC     HL
                LD      (HSTTRK), HL
NOWRAP:
                LD      (HSTSEC), DE

		LD	A,(secNo)
		INC	A
		LD	(secNo),A
		CP	$40
		JR	NZ, processSectorA

		CALL	printInline
		.DB CR,LF
		.TEXT "Formatting complete"
		.DB CR,LF,0

HANG:           JP      HANG

PRINTOP:        CALL    printInline
                .DB "Write Track ", 0
                CALL    OUTHXHL
                CALL    printInline
                .DB " Sector ",0
                CALL    OUTHXDE
                CALL    printInline
                .DB  CR, LF, 0
                RET

PRINTRES:       CALL    printInline
                .DB "Result ", 0
                CALL    OUTHXA
                CALL    printInline
                .DB  CR, LF, 0
                RET

;================================================================================================
; Utilities
;================================================================================================

printInline:
		EX 	(SP),HL 	; PUSH HL and put RET ADDress into HL
		PUSH 	AF
		PUSH 	BC
nextILChar:	LD 	A,(HL)
		CP	0
		JR	Z,endOfPrint
		RST 	08H
		INC 	HL
		JR	nextILChar
endOfPrint:	INC 	HL 		; Get past "null" terminator
		POP 	BC
		POP 	AF
		EX 	(SP),HL 	; PUSH new RET ADDress on stack and restore HL
		RET

secNo:          .DB 0

; Directory data for 1 x 128 byte sector
dirData:
		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

; Directory data for 1 x 128 byte sector
dirData1:
		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

; Directory data for 1 x 128 byte sector
dirData2:
		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

; Directory data for 1 x 128 byte sector
dirData3:
		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

		.DB $E5,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$00,$00,$00,$00
		.DB $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

#include "fdstd.asm"

FDENABLE        .EQU    1            ; TRUE FOR FLOPPY SUPPORT
FDMODE          .EQU    FDMODE_SCOTT1    ; FDMODE_DIO, FDMODE_ZETA, FDMODE_DIDE, FDMODE_N8, FDMODE_DIO3
FDTRACE         .EQU    3               ; 0=SILENT, 1=FATAL ERRORS, 2=ALL ERRORS, 3=EVERYTHING (ONLY RELEVANT IF FDENABLE = TRUE)
FDMEDIA         .EQU    FDM144          ; FDM720, FDM144, FDM360, FDM120 (ONLY RELEVANT IF FDENABLE = TRUE)
FDMEDIAALT      .EQU    FDM720          ; ALTERNATE MEDIA TO TRY, SAME CHOICES AS ABOVE (ONLY RELEVANT IF FDMAUTO = TRUE)
FDMAUTO         .EQU    1            ; SELECT BETWEEN MEDIA OPTS ABOVE AUTOMATICALLY
DSKYENABLE      .EQU    0

#include "utils.asm"
#include "fdutil.asm"
#include "fd.asm"

                .END
