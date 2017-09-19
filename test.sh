#!/bin/sh

mkdir build
cd build
cmake ..
make
./lab-01 ../resources
cd ..
rm -rf build

