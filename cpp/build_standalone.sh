#!/bin/bash

rm -f test

gcc -DSTANDALONE_BUILD -o test -g tuning.c -lusb-1.0
