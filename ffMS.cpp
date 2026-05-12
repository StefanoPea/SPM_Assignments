#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <vector>
#include <algorithm>
#include <iostream>
#include <map>
#include <deque>
#include <hpc_helpers.hpp>
#include <utility.hpp>
#include <cmdline.hpp>
#include <ff/allocator.hpp>

using namespace ff;

int main(int argc, char *argv[]){

    long start=parseCommandLine(argc, argv);
    if (start<0) return -1;

    // Number of farm workers
    unsigned W = FFTHREADS-1;

    // 0 sequential, 1 parallel
    size_t mode = MODE;

    // Initialize the Array to be sorted 
    srand(time(0));
    Record* A = new Record[ARRAYSIZE];
    for(size_t i=0;i<ARRAYSIZE;++i) A[i].key = rand()%100;
    for(size_t i=0;i<ARRAYSIZE;++i) {
        A[i].payload = new char[RPAYLOAD];
        for(size_t j=0;j<RPAYLOAD;++j) {
            A[i].payload[j] = 'a' + (rand() % 26);
        }   
            A[i].payload[RPAYLOAD -1] = '\0';         
    }


    if (mode == 0){ // Sequential Mergesort

        TIMERSTART(timer_seq_Tot)        
        seqMergeSort(A, ARRAYSIZE);
        TIMERSTOP(timer_seq_Tot)

    } else{         // FastFlow parallel Mergesort

        // do not spawn more workers than the array size
        if(ARRAYSIZE < W){
            W = ARRAYSIZE;
        }

        TIMERSTART(timer_par_Tot)        
        sort_chunk_with_fastflow(A, ARRAYSIZE, W);
        TIMERSTOP(timer_par_Tot)   
        printf("%b\n",is_sorted(A, ARRAYSIZE)); // Check if the array is sorted
             
    }

    // cleanup
    for(size_t i=0;i<ARRAYSIZE;++i) {
        delete[] A[i].payload;
        A[i].payload = nullptr;
    }

    delete[] A;
    return 0;
}