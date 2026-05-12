#!/bin/bash
# run_experiments.sh

# TEST PER FASTFLOW SUL CLUSTER

#FF SIZES for testing
SIZES=($((2**5)) $((2**10)) $((2**14)) $((2**18)) $((2**20)) $((2**22)))

FFTHREADS=(1 2 4 8 16)

# Clean and build
make cleanall
make ffMS


echo "SEQUENTIAL FF RUN" >> output.txt
echo >> output.txt
for size in "${SIZES[@]}"; do
    echo "Running sequential FF with size $size" >> output.txt
    ./ffMS -s $size -r 4 -m 0 >> output.txt
done

echo "PARALLEL FF RUN" >> output.txt
echo >> output.txt
for n in 1 2 4 8 16; do
    for size in "${SIZES[@]}"; do
        echo "Running with $n threads and size $size" >> output.txt
        ./ffMS -s $size -r 4 -t $n -m 1 >> output.txt
    done    
done