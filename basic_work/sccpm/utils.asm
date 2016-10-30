OUTHXA: PUSH BC
        LD C, A
        CALL OUTHX
        POP BC
        RET

OUTHXB: PUSH BC
        LD C, B
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

OUTHXDE: PUSH AF
        LD      A, D
        CALL    OUTHXA
        LD      A, E
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