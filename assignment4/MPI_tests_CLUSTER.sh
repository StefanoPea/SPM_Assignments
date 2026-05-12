#!/bin/bash

# bash MPI_tests_CLUSTER.sh


srun --time=00:00:30 --mpi=pmix make -B all

SIZES=($((2**5)) $((2**10)) $((2**14)) $((2**18)) $((2**20)) $((2**22)) )
WEAK_SIZES=($((2**23)) $((2**24)) $((2**25)))
N_NODES=(1 2 4 8)  

# Tests execution
for nodes in "${N_NODES[@]}"; do
	for size in "${SIZES[@]}"; do
		echo >> mpi_output.txt
 		echo "Running MPI test with $nodes nodes and size $size" >> mpi_output.txt
		srun --time=00:03:00 --mpi=pmix --ntasks-per-node=1 --nodes=$nodes ./mpiMS -s $size -r 4 -t 16 >> mpi_output.txt
	done
done

echo >> mpi_output.txt
echo "MPI+FF WEAK SCALABILITY VALUES">> mpi_output.txt

echo >> mpi_output.txt
echo "Running MPI test with 2 nodes and size 2**23" >> mpi_output.txt
srun --time=00:03:00 --mpi=pmix --ntasks-per-node=1 --nodes=2 ./mpiMS -s $((2**23)) -r 4 -t 16 >> mpi_output.txt

echo >> mpi_output.txt
echo "Running MPI test with 4 nodes and size 2**24" >> mpi_output.txt
srun --time=00:03:00 --mpi=pmix --ntasks-per-node=1 --nodes=4 ./mpiMS -s $((2**24)) -r 4 -t 16 >> mpi_output.txt

echo >> mpi_output.txt
echo "Running MPI test with 8 nodes and size 2**25" >> mpi_output.txt
srun --time=00:03:00 --mpi=pmix --ntasks-per-node=1 --nodes=8 ./mpiMS -s $((2**25)) -r 4 -t 16 >> mpi_output.txt








