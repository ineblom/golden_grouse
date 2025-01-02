#!/bin/sh

clang++ -O3 -march=native -flto -std=c++17 -c *.cpp
ar rcs libimgui.a *.o
rm *.o