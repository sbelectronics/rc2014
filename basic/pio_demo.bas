10 BASE=&H60
20 AD=BASE : AC = BASE+1 : BD = BASE+2 : BC = BASE+3

100 print "1 - Count on port A"
110 print "2 - Read from port B, display console"
120 print "3 - Read from port B, output to port A"
200 print "selection ";
210 input A$
220 if A$="1" THEN GOTO 1000
230 if A$="2" THEN GOTO 2000
240 if A$="3" THEN GOTO 3000 
300 GOTO 100

1000 OUT AC, &H0F : REM port A to output mode
1010 FOR I=0 to 255
1020 OUT AD, I
1030 FOR J=0 to 32 : NEXT J : REM delay a bit
1040 NEXT I
1050 GOTO 1010

2000 OUT BC, &H4F : REM port B to input mode
2010 LAST = 999
2020 V=INP(BD)
2030 IF (V = LAST) GOTO 2020
2040 LAST=V
2050 PRINT V
2060 GOTO 2020

3000 OUT AC, &H0F : REM port A to output mode
3010 OUT BC, &H4F : REM port B to input mode
3020 OUT AD, INP(BD)
3030 GOTO 3020
