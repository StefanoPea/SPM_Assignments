#!/bin/bash

#threads for the cluster
NTHREADS=(1 2 4 8 16 32)

#size of the chunks
CHUNKSIZE=(1 2 4 8 16)


#------------------------------------------------------------------------------------------------------------
rm -rf /tmp/s.pea1
mkdir -p /tmp/s.pea1



# copy the file from the current directory to /tmp
cp bigfile.dat /tmp/s.pea1/bigfile.dat

# copy all the small files to /tmp
for i in {1..100}
do
    cp file$i.dat /tmp/s.pea1/file$i.dat
done

#------------------------------------------------------------------------------------------------------------


#test for the big file saved in /tmp
TOCOMP="/tmp/s.pea1/bigfile.dat"
TODECOMP="/tmp/s.pea1/bigfile.dat.zip"


TOCOMP2=$(for i in {1..100}; do echo -n "/tmp/s.pea1/file$i.dat "; done)
TODECOMP2=$(for i in {1..100}; do echo -n "/tmp/s.pea1/file$i.dat.zip "; done)

#-----------------------------------------------------------------------------------------------------------

# Clean and build
make cleanall
make 

# TESTS WITH 1 LARGE FILE
echo "TESTING WITH 1 BIG FILE" >> output.txt
echo >> output.txt

# Run sequential test
echo "SEQUENTIAL" >> output.txt
./minizseq -r 0 -q 0 -C 1 $TOCOMP >> output.txt
echo >> output.txt
./minizseq -r 0 -q 0 -D 1 $TODECOMP >> output.txt

echo >> output.txt

# Run parallel test with different number of threads and chunk size

echo "PARALLEL" >> output.txt
for i in "${NTHREADS[@]}"; do
    for j in "${CHUNKSIZE[@]}"; do
        echo "THREADS: $i CHUNKSIZE: $j" >> output.txt
        OMP_NUM_THREADS=$i ./minizpar -r 0 -q 0 -C 1 -B $j $TOCOMP >> output.txt
        echo >> output.txt
        OMP_NUM_THREADS=$i ./minizpar -r 0 -q 0 -D 1 -B $j $TODECOMP >> output.txt
        echo >> output.txt
    done
done


# TESTS WITH MANY SMALL FILES
echo "TESTING WITH MANY SMALL FILES" >> output2.txt
echo >> output2.txt

# Run sequential test
echo "SEQUENTIAL" >> output2.txt
./minizseq -r 0 -q 0 -C 1 $TOCOMP2 >> output2.txt
echo >> output2.txt
./minizseq -r 0 -q 0 -D 1 $TODECOMP2 >> output2.txt

echo >> output2.txt

# Run parallel test with different number of threads and chunk size

echo "PARALLEL" >> output2.txt
for i in "${NTHREADS[@]}"; do
    
    echo "THREADS: $i CHUNKSIZE: 6" >> output2.txt
    OMP_NUM_THREADS=$i ./minizpar -r 0 -q 0 -C 1 -B 6 $TOCOMP2 >> output2.txt
    echo >> output2.txt
    OMP_NUM_THREADS=$i ./minizpar -r 0 -q 0 -D 1 -B 6 $TODECOMP2 >> output2.txt
    echo >> output2.txt
    
done


rm -rf /tmp/s.pea1
