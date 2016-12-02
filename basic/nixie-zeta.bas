1 REM nixie-zeta.bas
2 REM by Scott Baker, http://www.smbaker.com/
3 REM Demo of a nixie tube counter on the Zeta 2

7 REM constants for nixie-tube bits (data, clock, latch)
8 DB=1 : CB = 2 : LB = 4 : AD = &H61

10 REM configure parallel port, port B as output
20 OUT &H63, &HC0

100 REM display a simple counter
110 I=0
120 X=I
125 GOSUB 7000
130 I=I+1
140 GOTO 120

4000 REM transfer_latch
4010 OUT AD, LB
4020 OUT AD, 0
4030 RETURN

5000 REM shift_bit, bit is in B
5010 OUT AD, B
5020 OUT AD, B + CB
5030 OUT AD, B
5040 RETURN

6000 REM shift_digit, MSB first, digit is in DG
6001 REM print dg
6005 B=0
6010 IF (DG and 8)<>0 THEN B=1
6020 GOSUB 5000
6030 DG=DG*2
6035 B=0
6040 IF (DG and 8)<>0 THEN B=1
6050 GOSUB 5000
6080 DG=DG*2
6085 B=0
6090 IF (DG and 8)<>0 THEN B=1
6100 GOSUB 5000
6110 DG=DG*2
6115 B=0
6120 IF (DG and 8)<>0 THEN B=1
6130 GOSUB 5000
6140 RETURN

7000 REM display value in X
7010 D0=X\1000
7020 X=X-(D0*1000)
7030 D1=X\100
7040 X=X-(D1*100)
7050 D2=X\10
7060 X=X-(D2*10)
7070 D3=X

7100 DG=D3
7110 GOSUB 6000
7120 DG=D2
7130 GOSUB 6000
7135 DG=0 : REM skip decimal points and LED
7136 GOSUB 6000
7137 GOSUB 6000
7140 DG=D1
7150 GOSUB 6000
7160 DG=D0
7170 GOSUB 6000
7180 GOSUB 4000
7190 RETURN





