1 REM clock.bas
2 REM by Scott Baker, http://www.smbaker.com/
3 REM Demonstrates use of BQ4845 RTC on Z80 RC2014 computer

5 REM set 24-hour mode
6 OUT &HCE, 2

10 LS=999
20 GOSUB 1000
30 if (LS = S) GOTO 100
40 LS = S
50 GOSUB 2000
60 print T$
70 REM for the display board, output seconds on the LEDs
80 OUT 0, S
100 GOTO 20

998 REM read the current time from the RTC
999 REM store it in the variables H, M, S.
1000 X=inp(&HC0)
1010 S=(X and 15) + INT(X/16)*10
1020 X=inp(&HC2)
1030 M=(X and 15) + INT(X/16)*10
1040 X=inp(&HC4)
1050 H=(X and 15) + (INT(X/16) and 3)*10
1060 RETURN

1999 REM format H, M, S into a string T$
2000 T$=""
2010 if (H>9) GOTO 2030
2020 T$=T$+"0"
2030 T$=T$+right$(str$(H),len(str$(H))-1)
2040 T$=T$+":"
2050 if (M>9) GOTO 2070
2060 T$=T$+"0"
2070 T$=T$+right$(str$(M),len(str$(M))-1)
2080 T$=T$+":"
2090 if (S>9) GOTO 2110
2100 T$=T$+"0"
2110 T$=T$+right$(str$(S),len(str$(S))-1)
2120 RETURN

2999 REM set the clock using H, M, S
3000 TS=INT(S/10)
3010 OS=S-(TS*10)
3020 OUT &HC0, TS*16 + OS
3030 TM=INT(M/10)
3040 OM=M-(TM*10)
3050 OUT &HC2, TM*16 + OM
3060 TH=INT(H/10)
3070 OH=H-(TH*10)
3080 OUT &HC4, TH*16 + OH
3090 RETURN
