#!/bin/bash

rm -f libusb_mic_array.a tuning.o

gcc -o tuning.o -c tuning.c 
ar rcs libusb_mic_array.a tuning.o
