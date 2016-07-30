1 REM Serial Echo Demo for the Dual-Serial Board
2 REM Reads characters from Port B and writes them to Port A

10 IF RSER(0)=0 GOTO 10
15 X=ISER(0)
20 PRINT CHR$(X) ;
30 GOTO 10