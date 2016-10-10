1 REM clock.bas
2 REM by Scott Baker, http://www.smbaker.com/
3 REM Demonstrates use of BQ4845 RTC on Z80 RC2014 computer
4 REM with VFD display.

5 REM set 24-hour mode
6 OUT &HCE, 2

7 GOSUB 4000 : REM initialize VFD
8 GOSUB 4100 : REM VFD cursor off
9 T$="Scotts VFD Clock"
10 GOSUB 4500 : REM VFD print string

15 LS=999
20 GOSUB 1000 : REM get time

30 if (LS = S) GOTO 100
40 LS = S
50 GOSUB 2000
60 X=4 : Y=1 : GOSUB 4400 : REM goto line 2
70 GOSUB 4500
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

4000 OUT 0, &H30  : REM function set - 8 bit
4010 OUT 1, 0     : REM max brightness
4020 OUT 0, 1     : REM clear display
4030 OUT 0, &H0F  : REM display on, cursor on, blink on
4040 RETURN

4099 REM turn cursor off
4100 OUT 0, &H0C  : REM display on, cursor off
4110 RETURN

4199 REM turn cursor on
4200 OUT 0, &H0F  : REM display on, cursor on, blin on
4210 RETURN

4299 REM clear screen
4300 OUT 0, 1
4310 RETURN

4399 REM goto X, Y
4400 DA = &H80 + (&H40 * Y) + X
4410 OUT 0, DA
4420 RETURN

4499 REM print string
4500 FOR I=1 to LEN(T$)
4510 OUT 1, ASC(MID$(T$, I, 1))
4520 NEXT I
4530 RETURN