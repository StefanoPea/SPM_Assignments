#include <stdio.h>
#include "mpi.h"
#include <utility.hpp> 
#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <vector>
#include <algorithm>
#include <iostream>
#include <map>
#include <deque>
#include <cmath>
#include <hpc_helpers.hpp> 
#include <chrono>
#include <cmdline.hpp>     
#include <ff/allocator.hpp>

using namespace ff;

int main(int argc, char *argv[]) {
    
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    
    int numP;
    MPI_Comm_size(MPI_COMM_WORLD, &numP);
    
    int my_id;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

    // Parse command line arguments
    long start_time_val = parseCommandLine(argc, argv);
    if (start_time_val < 0) {
        MPI_Finalize();
        return -1;
    }


    
    // ================ PHASE 0: Initialization ===================//

    unsigned W = FFTHREADS - 1;             // Number of worker threads for the farm 

    Record* A = nullptr;                    // Initial array to be sorted
    Record* local_A = nullptr;              // Array that will hold the local sorted arrays
       
    size_t local_A_size = ARRAYSIZE/numP;   // Number of Record elements for this process, assume equally divisible between processes
    
    //----------------- FASTFLOW VARIABLES ----------------------//
    
    unsigned W_farm = W; 

    if (local_A_size > 0) {
        if (local_A_size < W_farm) {
            W_farm = local_A_size;
        }
        if (W_farm == 0 && local_A_size > 0) W_farm = 1; // Ensure at least one worker
    }
  
    //----------------- MERGE VARIABLES -------------------------//

    bool active_in_merge = (local_A_size > 0 || numP == 1);
    if (numP == 1 && ARRAYSIZE == 0) active_in_merge = false;

    // Number of merge rounds needed to combine all chunks
    int merge_rounds = (numP > 1) ? static_cast<int>(log2(numP)) : 0;

    //------------------------------------------------------------//


    // the Master node initializes the array
    if (my_id == 0)  {
        // Initialize the array 
        if (ARRAYSIZE > 0) {
            A = new Record[ARRAYSIZE];
            srand(time(0) + my_id); 
            for(size_t i = 0; i < ARRAYSIZE; ++i) A[i].key = rand() % 100; 
            for(size_t i = 0; i < ARRAYSIZE; ++i) {
                A[i].payload = new char[RPAYLOAD]; 
                for(size_t j = 0; j < RPAYLOAD - 1; ++j) { 
                   A[i].payload[j] = 'a' + (rand() % 26);
                }
                A[i].payload[RPAYLOAD -1] = '\0'; 
            }
        }        
    }

    
    //======================== TIMER START ========================//

    TIMERSTART(timer_mpi_Tot) // Start the timer for the entire MPI process
    
    
    // If only one process, sort the entire array without using MPI
    if(numP == 1) {
        if (my_id == 0) {
            // If only one process, sort the entire array
            if (ARRAYSIZE > 0) {
                sort_chunk_with_fastflow(A, ARRAYSIZE, W_farm);
            }
            // Check if the array is sorted
            bool sorted = is_sorted(A, ARRAYSIZE);
            printf("Process %d final array is %ssorted.\n", my_id, sorted ? "" : "not ");
        }
        TIMERSTOP(timer_mpi_Tot) // Stop the timer for the entire MPI process

         // cleanup
        for(size_t i=0;i<ARRAYSIZE;++i) {
            A[i].payload;
            A[i].payload = nullptr;
        }
        delete[] A; // Clean up the master array
        MPI_Finalize();
        return 0;
    }

    // ================ PHASE 1: Scatter ===================//

    //calculate the size of the received/sent array and allocate it
    int send_recv_count_bytes = 0;
    if (local_A_size > 0) {
        send_recv_count_bytes = static_cast<int>(local_A_size * sizeof(Record));
        local_A = new Record[local_A_size];
    } else {
        local_A = nullptr;
    }

    MPI_Scatter(
        A,                          // sendbuf (significant only at root)
        send_recv_count_bytes,      // sendcount (number of BYTES to send to each process)
        MPI_BYTE,                   // send datatype
        local_A,                    // recvbuf
        send_recv_count_bytes,      // recvcount (number of BYTES this process receives)
        MPI_BYTE,                   // recv datatype
        0,                          // root process
        MPI_COMM_WORLD
    );


    // ================= Phase 2: Local Sort + Merge Rounds ==================//

    MPI_Request recv_request = MPI_REQUEST_NULL;
    MPI_Status recv_status;

    Record* old_received_chunk = nullptr; // Buffer to receive data from sender processes

    for (int s = 0; s < merge_rounds; ++s) {
        
        if (!active_in_merge) {
            continue; // If this process is no longer active, skip to the next round
        }

        int stride = 1 << s; // stride = 2^s
        int receiver_rank;
        size_t expected_chunk_size_from_sender_at_this_stage = (ARRAYSIZE / numP) * (size_t)stride;
        

        if ((my_id & stride) != 0) {    // This process is a SENDER, a sender is a process that has the bit at position s set to 1
            if (!active_in_merge) continue;

            if (s == 0) {   // this is the first step, so we do a local sort of the array
                
                float ff_time = 0.0; // FastFlow time for sorting
                sort_chunk_with_fastflow(local_A, local_A_size, W_farm, &ff_time);

            }   else{       // this is a merge step, we merge our local_A with old_received_chunk and store it in local_A
                
                mergeSortedChunks(local_A, local_A_size, old_received_chunk, expected_chunk_size_from_sender_at_this_stage/2);
            }

            receiver_rank = my_id - stride;

         
            // This is the final call for this node, so the Send can be blocking
            MPI_Send(local_A, static_cast<int>(local_A_size * sizeof(Record)),
                         MPI_BYTE, receiver_rank, 1, MPI_COMM_WORLD);
            delete[] local_A; local_A = nullptr;
            
            local_A_size = 0;
            active_in_merge = false;
            
        } else {        // This process is a RECEIVER 

            int sender_rank = my_id + stride; // calculate the id of the sender node at this stage

            Record* received_chunk = new Record[expected_chunk_size_from_sender_at_this_stage];


            // Non blocking receive to store the array sent at this stage
            MPI_Irecv(received_chunk, static_cast<int>(expected_chunk_size_from_sender_at_this_stage * sizeof(Record)),
            MPI_BYTE, sender_rank, 1, MPI_COMM_WORLD, &recv_request);


            if (s == 0 && local_A_size > 0) { // this is the first step, so we need to do a local sort
                float ff_time = 0.0; // FastFlow time for sorting
                sort_chunk_with_fastflow(local_A, local_A_size, W_farm, &ff_time);
            }
            
            else{

                // merge local_A with the array obtained at the step before
                mergeSortedChunks(local_A, local_A_size, old_received_chunk, expected_chunk_size_from_sender_at_this_stage/2);

            }

            MPI_Wait(&recv_request, &recv_status); // Wait for the data to arrive
                    
            old_received_chunk = received_chunk;
        }
    }

    if(my_id == 0) {    // Last merge round, sort the final local_A
        mergeSortedChunks(local_A, local_A_size, old_received_chunk, ARRAYSIZE/2, A);
    } 

    // ====================== Phase 3: Finalize results ============================//

    if(my_id == 0) {
        
        TIMERSTOP(timer_mpi_Tot) // Stop the timer for the entire MPI process

        //check if local_A is sorted

        bool sorted = is_sorted(local_A, local_A_size);
        printf("Master process %d final merged array is %ssorted.\n", my_id, sorted ? "" : "not ");

        // cleanup
        for(size_t i=0;i<ARRAYSIZE;++i) {
            delete[] local_A[i].payload;
            local_A[i].payload = nullptr;
        }
    }

    delete[] local_A;

    MPI_Finalize();
    
    return 0;
}
