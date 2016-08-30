; bank switch to bank1, then jmp 0
	
                .ORG $0000

loader:	        .EQU $8000
bank_port:      .EQU $38

                LD DE, do_it
	        LD HL, loader
                LD C, 16	; copy 16 bytes
	
copyloop:       LD A, (DE)
                INC DE
	        LD (HL), A
	        INC HL
	        DEC C
                JR NZ, copyloop
	        JP loader
	
do_it:	        LD A, 1
	        OUT $38, A
	        JP 0
