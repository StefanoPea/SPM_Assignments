#!/bin/bash

#SBATCH --job-name=FF	# Job name
#SBATCH --output=terminal.log	# Standard output log file
#SBATCH --error=error.log	# Standard error log file
#SBATCH --time=00:10:00		# Time limit (hh:mm:ss)
#SBATCH --nodes=1			# Number of nodes
#SBATCH --ntasks=1			# Number of total tasks (MPI processes)
#SBATCH --cpus-per-task=2	# Number of CPU cores per task
#SBATCH --partition=normal	# Partition to submit to

srun bash test_FF.sh


