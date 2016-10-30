;==================================================================================
; Floppy Putsys
; http://www.smbaker.com/
;
; Portions based on original source of Grant Searle (http://searle.hostei.com/grant/index.html)
;
;==================================================================================

LF		.EQU	0AH		;line feed
FF		.EQU	0CH		;form feed
CR		.EQU	0DH		;carriage RETurn

loadAddr	.EQU	0D000h
numSecs		.EQU	24	; Number of 512 sectors to be loaded


;====================================================================================

		.ORG	8100H		; Format program origin.

INIT            LD        HL,$F000        ;  lets get lots of stack space...
                LD        SP,HL           ; Set up a temporary stack

		CALL	printInline
		.TEXT "CP/M Putsys for 1.44 floppy"
		.DB CR,LF,0

                CALL    FD_INIT

                LD      B, DOP_READID
                CALL    FD_DISPATCH

                CALL    printInline
                .TEXT   "Media ",0
                CALL    OUTHXA
                CALL    printInline
                .TEXT   CR, LF, 0

                ; point disk buffer to load address
                LD      HL, loadAddr
                LD      (DIOBUF), HL

                ; start at block $0, which is Track 1 Sector 0
                ;    (track numbers start at 1, sectors start at 0)
                LD      HL, 1
                LD      (HSTTRK), HL
                LD      HL, $0
                LD      (HSTSEC), HL

                ; number of sectors to write is in B
                LD      B, numSecs

processSectors:
                ; write the sector

                ; C = disk num
                ; HL = track
                ; DE = sec
                ; B = function

                PUSH    BC

                LD      C, 0
                LD      HL, (HSTTRK)
                LD      DE, (HSTSEC)
                LD      B, DOP_WRITE

                call    PRINTOP

                call    FD_DISPATCH

                call    PRINTRES

                POP     BC

                ; increment sector

                LD      DE, (HSTSEC)
                INC     DE
                LD      A, E
                CP      18
                JR      C, NOWRAP
                LD      DE, 0
                LD      HL, (HSTTRK)
                INC     HL
                LD      (HSTTRK), HL
NOWRAP:
                LD      (HSTSEC), DE

                ; increment load address

		LD	DE,0200H
		LD	HL,(DIOBUF)
		ADD	HL,DE
		LD	(DIOBUF),HL
                djnz	processSectors

		CALL	printInline
		.DB CR,LF
		.TEXT "Putsys complete"
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

#include "fdstd.asm"

FDENABLE        .EQU    1            ; TRUE FOR FLOPPY SUPPORT
FDMODE          .EQU    FDMODE_SCOTT1    ; FDMODE_DIO, FDMODE_ZETA, FDMODE_DIDE, FDMODE_N8, FDMODE_DIO3
FDTRACE         .EQU    0               ; 0=SILENT, 1=FATAL ERRORS, 2=ALL ERRORS, 3=EVERYTHING (ONLY RELEVANT IF FDENABLE = TRUE)
FDMEDIA         .EQU    FDM144          ; FDM720, FDM144, FDM360, FDM120 (ONLY RELEVANT IF FDENABLE = TRUE)
FDMEDIAALT      .EQU    FDM720          ; ALTERNATE MEDIA TO TRY, SAME CHOICES AS ABOVE (ONLY RELEVANT IF FDMAUTO = TRUE)
FDMAUTO         .EQU    1            ; SELECT BETWEEN MEDIA OPTS ABOVE AUTOMATICALLY
DSKYENABLE      .EQU    0

#include "utils.asm"
#include "fdutil.asm"
#include "fd.asm"

                .END
