1 REM Serial Echo Demo for the Dual-Serial Board
2 REM Reads characters from port 1 and write to port 0

10 REM check port 1
20 IF RSER(1)=0 GOTO 50
30 X=ISER(1)
40 OSER 0,X
50 GOTO 10
