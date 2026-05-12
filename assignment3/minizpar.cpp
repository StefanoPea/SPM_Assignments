#include <config.hpp>
#include <omp.h>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <sys/stat.h>
#include <iostream>
#include "hpc_helpers.hpp"
#include "my_utils.hpp"
#include "cmdline.hpp"

int main(int argc, char *argv[]) {
   

    // openMP settings
    omp_set_dynamic(0);     // Disable dynamic teams to ensure a fixed number of threads 
    omp_set_nested(1);      // Enable nested parallelism  

    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    // parse command line arguments and set some global variables
    long start=parseCommandLine(argc, argv);
    if (start<0) return -1;
  
	bool success = true;

    // start the timer
    double timer_start = omp_get_wtime();
    
    // process the files in parallel 
    #pragma omp parallel for reduction(&&:success)
    for (int i = start; i < argc; i++) {
        if (QUITE_MODE >= 2) {
            #pragma omp critical
            {
                std::cout << "Processing file: " << argv[i] << "\n";
            }
        }
        size_t filesize=0;
		if (isDirectory(argv[i], filesize)) {
			success &= walkDir(argv[i],COMP);
		} else {
			success &= doWorkPar(argv[i], filesize,COMP);   // function to compress or decompress a file
		} 
    }

    double timer_stop = omp_get_wtime();
    double elapsed = timer_stop - timer_start;
    std::cout << "# elapsed time (parTimer): " << elapsed << "s\n";

	if (!success) {
		printf("Exiting with (some) Error(s)\n");
		return -1;
	}
	printf("Exiting with Success\n");
	return 0;
}

