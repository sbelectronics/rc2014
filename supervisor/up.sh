#! /bin/bash

# copies supervisor and necessary libraries to my raspberry pi

scp ../../nixiecalc/ioexpand.py getch.py supervisor.py ../tools/hexfile.py pi@198.0.0.233:/home/pi/
