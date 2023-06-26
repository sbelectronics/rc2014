5 VD = 1 : VC = 0 : REM data port = 1, control port = 0
10 GOSUB 4000     : REM initialize
20 GOSUB 4100     : REM cursor off
30 T$ = "Hello, World"
40 GOSUB 4500     : REM print string

999 GOTO 999

4000 OUT VC, &H30  : REM function set - 8 bit
4010 OUT VD, 0     : REM max brightness
4020 OUT VC, 1     : REM clear display
4030 OUT VC, &H0F  : REM display on, cursor on, blink on
4040 RETURN

4099 REM turn cursor off
4100 OUT VC, &H0C  : REM display on, cursor off
4110 RETURN

4199 REM turn cursor on
4200 OUT VC, &H0F  : REM display on, cursor on, blin on
4210 RETURN

4299 REM clear screen
4300 OUT VC, 1
4310 RETURN

4399 REM goto X, Y
4400 DA = &H80 + (&H40 * Y) + X
4410 OUT VC, DA
4420 RETURN

4499 REM print string
4500 FOR I=1 to LEN(T$)
4510 OUT VD, ASC(MID$(T$, I, 1))
4520 NEXT I
4530 RETURN