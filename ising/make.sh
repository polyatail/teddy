#!/bin/bash

gcc -I/usr/include/directfb -g -O2 -Wall -c simple.c
gcc -g -O2 -Wall -o simple simple.o -ldirectfb -lfusion -ldirect -lpthread -lm

