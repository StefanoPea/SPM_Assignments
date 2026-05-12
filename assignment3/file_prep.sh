#!/bin/bash

#Used to generate 100 files of 10 KiB

for i in {1..100}
do
    dd if=/dev/urandom of=file$i.dat bs=1024 count=10

done
