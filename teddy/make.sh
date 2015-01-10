#!/bin/bash

g++ -I/usr/include -g -O2 -Wall tones.c -o tones -lm -lboost_system -lboost_thread -lportaudio -lsndfile
