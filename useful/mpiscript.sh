
# SOLO PER TEST SULLA MIA MACCHINA


# Clean and build
make cleanall
#make all
make mpiMS

#mpirun -n 1  ./mpiMS -s 21 -r 4 -t 4 -m 1 : -n 1 valgrind ./mpiMS -s 21 -r 4 -t 4 -m 1

mpirun -n 8 --oversubscribe ./mpiMS -s $((2**18)) -r 4 -t 4 -m 1 >> mpioutput.txt


#mpirun -n 2 --oversubscribe ./mpiMS -s 16 -r 4 -t 4 -m 1 >> mpioutput.txt