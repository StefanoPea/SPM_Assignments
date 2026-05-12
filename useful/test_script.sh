# Clean and build
make cleanall
#make all
make mpiMS


# sequential test 
#./ffMS -s 1000000 -r 4 -t 6 -m 0

# parallel test only ff
#./ffMS -s 1000000 -r 4 -t 6 -m 1

# parallel test ff+mpi
#srun --mpi=pmix -n 8  --ntasks-per-node 1 ./mpiMS -s 4194304 -r 4 -t 16 -m 1 >> mpioutput.txt
srun --mpi=pmix -n 4  --ntasks-per-node 1 ./mpiMS -s 4194304 -r 4 -t 16 -m 1 >> mpioutput.txt
