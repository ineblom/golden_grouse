#!/bin/sh

clang -O3 -march=native -flto -I ./include -c ./src/*.c
ar rcs libglad.a *.o
rm *.o
