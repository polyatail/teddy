#!/bin/bash

g++ -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -Wall -lrf24-bcm rf24.cpp -o rf24
