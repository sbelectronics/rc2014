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

1000 OUT AC, &HCF : REM port A to control mode
1005 OUT AC, &H00 : REM port A all pins output
1010 FOR I=0 to 255
1020 OUT AD, I
1030 FOR J=0 to 32 : NEXT J : REM delay a bit
1040 NEXT I
1050 GOTO 1010

2000 OUT BC, &HCF : REM port B to control mode
2005 OUT BC, &HFF : REM port B all pins input
2010 LAST = 999
2020 V=INP(BD)
2030 IF (V = LAST) GOTO 2020
2040 LAST=V
2050 PRINT V, "buttons pressed: ";
2060 IF (V and 1)=0 THEN PRINT "1 ";
2070 IF (V and 2)=0 THEN PRINT "2 ";
2080 IF (V and 4)=0 THEN PRINT "3 ";
2090 IF (V and 8)=0 THEN PRINT "4 ";
2100 IF (V and 16)=0 THEN PRINT "5 ";
2110 IF (V and 32)=0 THEN PRINT "6 ";
2120 IF (V and 64)=0 THEN PRINT "7 ";
2130 IF (V and 128)=0 THEN PRINT "8 ";
2140 PRINT ""
2150 GOTO 2020

3000 OUT AC, &HCF : REM port A to control mode
3005 OUT AC, &H00 : REM port A all pins output
3010 OUT BC, &HCF : REM port B to control mode
3015 OUT BC, &HFF : REM port B all pins input
3020 OUT AD, INP(BD)
3030 GOTO 3020
