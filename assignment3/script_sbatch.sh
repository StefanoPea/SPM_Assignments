#!/bin/bash

#SBATCH --job-name=minizpar	# Job name
#SBATCH --output=output.log	# Standard output log file
#SBATCH --error=error.log	# Standard error log file
#SBATCH --time=00:15:00		# Time limit (hh:mm:ss)
#SBATCH --nodes=1			# Number of nodes
#SBATCH --ntasks=1			# Number of total tasks (MPI processes)
#SBATCH --cpus-per-task=2	# Number of CPU cores per task
#SBATCH --partition=normal	# Partition to submit to

srun bash script.sh
