#!/bin/bash

gcc -I/usr/include/directfb -g -O2 -Wall -c ising.c
gcc -g -O2 -Wall -o ising ising.o -ldirectfb -lfusion -ldirect -lpthread -lm

