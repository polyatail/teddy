#!/bin/bash

g++ -I/usr/local/Cellar/boost/1.56.0/include -L/usr/local/Cellar/boost/1.56.0/lib \
    -I/usr/local/Cellar/portaudio/19.20140130/include -L/usr/local/Cellar/portaudio/19.20140130/lib \
    -g -O2 -Wall tones.c -o tones -lboost_system -lboost_thread-mt -lportaudio
