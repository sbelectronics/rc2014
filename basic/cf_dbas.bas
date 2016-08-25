10 REM compactflash demo, www.smbaker.com
15 REM This demo requires Scott's Disk Basic. 
20 REM Disk basic puts a sector buffer at $8095.
25 REM You can peek and poke to this buffer.
30 REM Then call DREAD or DWRITE to read or write the buffer to a sector 
35 REM number.

50 CLEAR 1000 : REM reserve some string space

60 DINIT : REM initialize the compactflash driver

100 print "1 - dump drive id"
110 print "2 - read sector"
120 print "3 - write sector"
130 print "4 - fill sector with zero"
140 print "5 - print disk id"
200 print "selection ";
210 input A$
220 if A$="1" THEN GOSUB 8400
230 if A$="2" THEN GOSUB 1000
240 if A$="3" THEN GOSUB 2000
250 if A$="4" THEN GOSUB 3000
260 if A$="5" THEN GOSUB 4000
300 GOTO 100

1000 print "sector ";
1010 INPUT S
1020 DREAD S
1030 CT=512 : GOSUB 8600
1040 RETURN

2000 print "sector ";
2010 INPUT S
2020 print "data ";
2030 INPUT D$
2040 DREAD S
2050 FOR I=1 TO LEN(D$)
2060 POKE &H8095+I, ASC(MID$(D$,I,1))
2070 NEXT I
2080 DWRITE S
2090 RETURN

3000 print "sector ";
3010 INPUT S
3050 FOR I=1 TO 512
3060 POKE &H8095+I, 0
3070 NEXT I
3080 DWRITE S
3090 RETURN

4000 DINIT
4010 CT=512 : GOSUB 8600
4020 RETURN

8600 REM dump data from disk buffer
8610 FOR I=0 TO (CT/16)-1
8620 X=I*16 : GOSUB 8850 : PRINT " " ; : REM print offset
8630 AS$=""
8640 FOR J=1 to 16
8650 X=PEEK(&H8095 + I*16 + J)
8660 GOSUB 8800
8670 PRINT " ";
8680 IF (X>32) AND (X<128) THEN AS$=AS$+CHR$(X) : GOTO 8700
8690 AS$=AS$+"."
8700 NEXT J
8710 PRINT AS$
8720 NEXT I
8730 RETURN

8800 REM print two hex digits
8810 Y=INT(X/16) : GOSUB 8900
8820 Y=X-(Y*16) : GOSUB 8900
8830 RETURN

8849 REM print four hex digits
8850 Z=X
8860 X=INT(Z/256) : GOSUB 8800
8870 X=Z-(X*256) : GOSUB 8800
8880 X=Z
8890 RETURN

8900 REM print one hex digit
8910 IF (Y>=10) THEN PRINT CHR$(Y-10+65) ; : RETURN
8920 PRINT CHR$(Y+48) ;
8930 RETURN

