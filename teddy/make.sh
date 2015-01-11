#!/bin/bash

g++ -I/usr/include -g -Ofast -Wall tones.c -o tones -lm -lboost_system -lboost_thread -lportaudio -lsndfile
