1 REM clock.bas
2 REM by Scott Baker, http://www.smbaker.com/
3 REM Demonstrates use of BQ4845 RTC on Z80 RC2014 computer

8 DB=1 : CB = 2 : LB = 4

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
1050 H=(X and 15) + INT(X/16)*10
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

4000 REM transfer_latch
4010 OUT &HE0, LB
4020 OUT &HE0, 0
4030 RETURN

5000 REM shift_bit, bit is in B
5010 OUT &HE0, B
5020 OUT &HE0, B + CB
5030 OUT &HE0, B
5040 RETURN

6000 REM shift_digit, MSB first, digit is in DG
6010 IF (DG and 8) THEN B=1 ELSE B = 0
6020 GOSUB 5000
6030 DG=DG*2
6040 IF (DG and 8) THEN B=1 ELSE B = 0
6050 GOSUB 5000
6080 DG=DG*2
6090 IF (DG and 8) THEN B=1 ELSE B = 0
6100 GOSUB 5000
6110 DG=DG*2
6120 IF (DG and 8) THEN B=1 ELSE B = 0
6130 GOSUB 5000
6140 RETURN

7000 REM display_nixie_clock, time is in H, M, and S
7010 TH=INT(H/10)
7020 OH=H-(TH*10)
7030 TM=INT(M/10)
7040 OM=M-(TS*10)
7050 TS=INT(S/10)
7060 OS=S-(TS*10)
7070 DG=TH
7080 GOSUB 6000
7090 DG=OH
7100 GOSUB 6000
7110 DG=TM
7120 GOSUB 6000
7130 DG=OM
7140 GOSUB 6000
7150 DG=TS
7160 GOSUB 6000
7170 DG=OS
7180 GOSUB 6000
7190 GOSUB 4000
7200 RETURN




