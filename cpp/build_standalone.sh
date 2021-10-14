#!/bin/bash

rm -f test

g++ -DSTANDALONE_BUILD -o test -g tuning.cpp -lusb-1.0
