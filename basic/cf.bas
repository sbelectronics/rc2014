10 REM compactflash demo, www.smbaker.com
20 REM This was intended to be a basic program that could read and write
30 REM the compactflash card. Unfortunately, doing a sector write on a fuji
40 REM card using basic appears to be too slow and aborts. 

50 GOSUB 8000 : REM configure

100 print "1 - dump drive id"
110 print "2 - read sector"
120 print "3 - write sector"
200 print "selection ";
210 input A$
220 if A$="1" THEN GOSUB 8400
230 if A$="2" THEN GOSUB 1000
240 if A$="3" THEN GOSUB 2000 
300 GOTO 100

1000 print "sector ";
1010 INPUT S
1020 GOSUB 8500
1030 RETURN

2000 print "sector ";
2010 INPUT S
2020 print "data ";
2030 INPUT D$
2040 GOSUB 9000
2050 RETURN

8000 REM setup constants for IDE port addresses
8010 I0=&HE0
8020 I1=I0+1
8030 I2=I0+2
8040 I3=I0+3
8050 I4=I0+4
8060 I5=I0+5
8070 I6=I0+6
8080 I7=I0+7
8090 IF (INP(I7) AND &H80)<>0 GOTO 8090 : REM wait for not busy
8100 OUT I1, 1    : REM 8-bit mode
8110 OUT I7, &HEF : REM set features command
8120 OUT I1, &H82 : REM no-cache
8130 OUT I7, &HEF : REM set features command 
8190 RETURN

8200 REM wait-for-ready
8210 X=INP(I7)
8220 IF (X AND &H40)=0 GOTO 8210  : REM wait for ready=1
8230 IF (X AND &H80)<>0 GOTO 8210  : REM wait for busy=0
8240 RETURN

8300 REM wait-for-drq
8310 X=INP(I7)
8320 IF (X AND &H08)=0 GOTO 8310 : REM wait for drq=1
8330 IF (X AND &H80)<>0 GOTO 8310 : REM wait for busy=0
8340 RETURN

8400 REM read drive status
8410 GOSUB 8200 : REM wait for ready
8420 OUT I6, &HE0 : REM select master 
8430 OUT I7, &HEC : REM send drive id command
8440 GOSUB 8300 : REM wait for drq
8450 CT=512
8460 GOSUB 8600
8470 RETURN

8500 REM read sector
8510 GOSUB 8200 : REM wait for ready
8515 OUT I2, 1
8520 OUT I3, S-(INT(S/256)*256)
8530 OUT I4, INT(S/256)
8535 OUT I5, 0
8540 OUT I6, &HE0 : REM select master
8550 OUT I7, &H20 : REM send drive id command
8560 GOSUB 8300 : REM wait for drq
8570 CT=512
8580 GOSUB 8600
8590 RETURN

8600 REM dump data from disk to screen
8610 FOR I=0 TO (CT/16)-1
8620 X=I*16 : GOSUB 8850 : PRINT " " ; : REM print offset
8630 AS$=""
8640 FOR J=1 to 16
8650 X=INP(I0)
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

9000 REM read sector
9010 GOSUB 8200 : REM wait for ready
9215 OUT I2, 1
9020 OUT I3, S-(INT(S/256)*256)
9030 OUT I4, INT(S/256)
9035 OUT I5, 0
9040 OUT I6, &HE0 : REM select master
9050 OUT I7, &H30 : REM send drive write command
9060 GOSUB 8300 : REM wait for drq
9070 FOR I=1 to LEN(D$)
9080 OUT I0, ASC(MID$(D$,I,1))
9090 NEXT I
9100 FOR I=1 to 512-LEN(D$)
9110 OUT I0, 0
9120 NEXT I
9130 RETURN
