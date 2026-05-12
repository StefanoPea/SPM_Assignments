#!/bin/bash

#SOLO PER TEST SULLA MIA MACCHINA


SIZES=(10 100 1000 10000 100000 1000000 10000000 100000000)
FFTHREADS=(1 2 4 8 16 32)
RPAYLOADS=(8 16 32 64 128 256)


# Clean and build
make cleanall
#make all
make ffMS

#echo "TESTING SEQUENTIAL MERGESORT" >> output.txt
#echo >> output.txt

#./ffMS -s 5 -r 4 -t 4 -m 0 >> output.txt

#echo >> output.txt

# echo >> output.txt

#./ffMS -s 4194304 -r 4 -t 4 -m 0 >> output.txt


#./ffMS -s 4194304 -r 4 -t 4 -m 1 >> output.txt



#./ffMS -s 16 -r 4 -t 4 -m 0 >> output.txt


./ffMS -s 16 -r 4 -t 4 -m 1 >> output.txt












#valgrind \
#  --leak-check=full \
#  --show-leak-kinds=all \
#  --track-origins=yes \
#  --fair-sched=yes \
#  --log-file=valgrind.log \
#  ./test -s 100 -r 8 -t 4 -m 1 >> output.txt

#echo "TESTING PARALLEL MERGESORT" >> output.txt
#echo >> output.txt
#valgrind \
#  --leak-check=full \
#  --show-leak-kinds=all \
#  --track-origins=yes \
#  --verbose \
#  --log-file=valgrind.log \
#    ./test -s 1000 -r 8 -t 4 -m 1 >> output.txt


# Run sequential test
#echo "SEQUENTIAL" >> output.txt
#./collatz_seq "${RANGES[@]}" >> output.txt
#echo >> output.txt
