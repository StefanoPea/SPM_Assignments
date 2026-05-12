#!/bin/bash

RANGES=(1000000000000000000-1000000000010000000 100000-110000 1000000-2000000)
THREADS=(1 2 4 8 16 32)
CHUNK_SIZES=(8 16 32 64 128 256 512 1024 2048)


# Clean and build
make cleanall
make -B collatz_seq
make -B collatz_par


# TESTS WITH 1 LARGE RANGE and 2 small range
echo "TESTING WITH 1 LARGE RANGE" >> output.txt
echo >> output.txt

# Run sequential test
echo "SEQUENTIAL" >> output.txt
./collatz_seq "${RANGES[@]}" >> output.txt
echo >> output.txt

# Run parallel static tests
echo "PARALLEL STATIC" >> output.txt
for n in "${THREADS[@]}"; do
    for c in "${CHUNK_SIZES[@]}"; do
        echo "Threads: $n, Chunk Size: $c" >> output.txt
        ./collatz_par -c $c -n $n "${RANGES[@]}" >> output.txt
        echo >> output.txt
    done
done

# Run parallel dynamic tests
echo "PARALLEL DYNAMIC" >> output.txt
for n in "${THREADS[@]}"; do
    for c in "${CHUNK_SIZES[@]}"; do
        echo "Threads: $n, Chunk Size: $c" >> output.txt
        ./collatz_par -d -c $c -n $n "${RANGES[@]}" >> output.txt
        echo >> output.txt
    done
done
