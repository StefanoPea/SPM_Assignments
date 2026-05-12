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

// TODO ALLOCATE ONLY ONCE THE RECEIVE BUFF
// DELETE THE RECEIVE BUFF


using namespace ff;

int main(int argc, char *argv[]) {
    
    MPI_Init(&argc, &argv);
    
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

    //===================== TESTTTTTT =================================

    // INSIDE main(), BEFORE the merge loop

    Record* received_chunk = nullptr;
    size_t received_chunk_capacity = 0;


    

    size_t chunk_size_max = 0;
    if (numP > 1) {
        chunk_size_max = (ARRAYSIZE / numP) * (numP / 2);
    }
    // If numP==1, no merges happen; chunk_size_max remains 0.

    // Allocate two buffers of that max size (double buffering)
    Record *buffer1 = nullptr, *buffer2 = nullptr;
    if (chunk_size_max > 0) {
        buffer1 = new Record[chunk_size_max];
        buffer2 = new Record[chunk_size_max];
    }
    // We'll ping-pong between these two inside the loop.



    
    // ================ PHASE 0: Initialization ===================//

    unsigned W = FFTHREADS - 1; // Number of worker threads for the farm 

    Record* A = nullptr;      // Initial array to be sorted
    Record* local_A = nullptr;     // Array that will hold the local sorted arrays
       
    size_t local_A_size = ARRAYSIZE/numP; // Number of Record elements for this process, assume power of 2 for simplicity


    
    //----------------- FASTFLOW VARIABLES ----------------------//
    unsigned W_farm = W; 

    if (local_A_size > 0) {
        if (local_A_size < W_farm) {
            W_farm = local_A_size;
        }
        if (W_farm == 0 && local_A_size > 0) W_farm = 1; // Ensure at least one worker
    }
    //-----------------------------------------------------------//

    //============= MERGE VARIABLES ========================//

    bool active_in_merge = (local_A_size > 0 || numP == 1);
    if (numP == 1 && ARRAYSIZE == 0) active_in_merge = false;

    int merge_rounds = (numP > 1) ? static_cast<int>(log2(numP)) : 0;

    //=========================================================


    if (my_id == 0)  {
        
        // Initialize the array 
        if (ARRAYSIZE > 0) {
            A = new Record[ARRAYSIZE];
            srand(time(0) + my_id); 
            for(size_t i = 0; i < ARRAYSIZE; ++i) A[i].key = rand() % 100; 
            for(size_t i = 0; i < ARRAYSIZE; ++i) {
                for(size_t j = 0; j < RPAYLOAD - 1; ++j) { 
                   A[i].payload[j] = 'a' + (rand() % 26);
                }
                A[i].payload[RPAYLOAD -1] = '\0'; 
            }
        }

        //printf("Master process %d initialized array of size %zu.\n", my_id, ARRAYSIZE);
        ////if (ARRAYSIZE > 0 && A != nullptr) { 
        //    printf("Master process %d initial array KEYS: ", my_id);
        //    for(size_t i = 0; i < ARRAYSIZE ; ++i) printf("%lu ", A[i].key);
        //    printf("\n");
        //    //print the PAYLOAD
        //    printf("Master process %d initial array PAYLOADS: ", my_id);
        //    for(size_t i = 0; i < ARRAYSIZE ; ++i) printf("%s ", A[i].payload);
        //    printf("\n");
////
        ////}


        //print the last 3 keys of the final merged array
        //printf("Master process %d initial array KEYS: ", my_id);
        //for(size_t i = 0; i < ARRAYSIZE; ++i) {
        //    printf("%lu ", A[i].key);
        //}
        //printf("\n");
        //printf("Master process %d initial array PAYLOADS: ", my_id);
        //for(size_t i = 0; i < ARRAYSIZE; ++i) {
        //    printf("%s ", A[i].payload);
        //}
        //printf("\n");
    }

    


    //======================== TIMER START ========================//

    TIMERSTART(timer_mpi_Tot) // Start the timer for the entire MPI process
    
    
    //=============================================================//
   
    // --- Phase numP == 1 ---


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
        delete[] A; // Clean up the master array
        MPI_Finalize();
        TIMERSTOP(timer_mpi_Tot) // Stop the timer for the entire MPI process
        return 0;
    }




    // --- Phase 1: ScatterV ---

    //printf("Process %d: Starting scatter operation with local_A_size = %zu records.\n", my_id, local_A_size);


    
    int send_recv_count_bytes = 0;
    if (local_A_size > 0) { // Ensure we don't multiply by zero if sizeof(Record) is large
        send_recv_count_bytes = static_cast<int>(local_A_size * sizeof(Record));
    }


    TIMERSTART(local_A_allocation)// Start the timer for local_A allocation
    //TODO THIS IS A BIG BOTTLENECK
    // Each process allocates its receive buffer
    if (local_A_size > 0) {
        local_A = new Record[local_A_size];
    } else {
        local_A = nullptr;
    }

    
    if(my_id == 0){TIMERSTOP(local_A_allocation)} // Stop the timer for local_A allocation

    TIMERSTART(scatter_operation)// Start the timer for the scatter operation
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
    if(my_id == 0){TIMERSTOP(scatter_operation)} // Stop the timer for the scatter operation





    //if(my_id == 0){
    //    printf("Master process %d initial local_A KEYS: ", my_id);
    //    for(size_t i = 0; i < local_A_size; ++i) {
    //        printf("%lu ", local_A[i].key);
    //    }
    //    printf("\n");
    //}


    TIMERSTART(local_A_SORTING) // Start the timer for local_A sorting

    //==========================================================================================

    // --- Phase 3 + 4: FF Farm + Gather results ---


    //printf("Process %d: Initial local_A_size = %zu records.\n", my_id, local_A_size);
   

    //=========================RECEIVER VARIABLES=================================
    //TODO HO COMMENTATO QUESTO NEL TEST 'TESTTTTTT'
    //Record* received_chunk = nullptr;
    MPI_Request recv_request = MPI_REQUEST_NULL;
    MPI_Status recv_status;
    //=========================================================================


    for (int s = 0; s < merge_rounds; ++s) {
        
        if (!active_in_merge) {
            continue; // If this process is no longer active, skip to the next round
        }

        int stride = 1 << s; // stride = 2^s
        int receiver_rank;
        size_t expected_chunk_size_from_sender_at_this_stage = (ARRAYSIZE / numP) * (size_t)stride;
        

        if ((my_id & stride) != 0) { // This process is a SENDER
            if (!active_in_merge) continue;

            if (s == 0) {
                // First time this process acts (as a sender). Sort its initial chunk.
                //printf("Step %d: P%d (Sender) sorting its initial chunk of size %zu before sending to P%d.\n", s, my_id, current_data_size_records, partner_rank);
                sort_chunk_with_fastflow(local_A, local_A_size, W_farm);
            }   

            receiver_rank = my_id - stride;
            //printf("Step %d: P%d (Sender) sending %zu records to P%d (Receiver)\n", s, my_id, local_A_size, receiver_rank);

            // No need to send size if receiver can calculate it

            if (local_A_size > 0) { // local_A_size should be expected_chunk_size_from_sender_at_this_stage
                MPI_Send(local_A, static_cast<int>(local_A_size * sizeof(Record)),
                         MPI_BYTE, receiver_rank, 1, MPI_COMM_WORLD); // Blocking Send
                delete[] local_A; local_A = nullptr;
            }
            local_A_size = 0;
            active_in_merge = false;



        } else { // This process is a RECEIVER (my_id's s-th bit is 0)


            int sender_rank = my_id + stride;

            //TODO TESTTTTTTTTTTTT
            //received_chunk = new Record[expected_chunk_size_from_sender_at_this_stage];


            

            MPI_Irecv(received_chunk, static_cast<int>(expected_chunk_size_from_sender_at_this_stage * sizeof(Record)),
            MPI_BYTE, sender_rank, 1, MPI_COMM_WORLD, &recv_request);

            if (s == 0 && local_A_size > 0) {
                // This is the first merge round for this receiver. Sort its *own* initial chunk.
                //printf("Step %d: P%d (Receiver) sorting its initial chunk of size %zu while waiting for P%d.\n", s, my_id, current_data_size_records, partner_rank);
                sort_chunk_with_fastflow(local_A, local_A_size, W_farm);
            }
            // For s > 0, local_A is already sorted from a previous merge. No "starting chunk" sort here.
            // Other computation could be placed here if available.

            else{
                mergeSortedChunks(local_A, local_A_size, received_chunk, expected_chunk_size_from_sender_at_this_stage);

            }

            MPI_Wait(&recv_request, &recv_status); // Wait for the data to arrive
            //printf("Step %d: P%d (Receiver) received %zu records from P%d.\n", s, my_id, actual_records_to_receive, partner_rank);

                    

            // Merge local_A (sorted) with received_chunk (should also be sorted by sender)            


        }
    }

    if(my_id==0) {TIMERSTOP(local_A_SORTING)} // Start the timer for local_A sorting



    // --- Phase 5: Finalize results ---
    // After all merge rounds, the master process (my_id == 0) will have the final merged array 

    if(my_id == 0) {
        


        //======================== TIMER STOP =================================//

        TIMERSTOP(timer_mpi_Tot) // Stop the timer for the entire MPI process

        
        //=======================================================================//


        //check if local_A is sorted

        bool sorted = is_sorted(local_A, local_A_size);
        printf("Master process %d final merged array is %ssorted.\n", my_id, sorted ? "" : "not ");


        delete[] A; // Clean up the master array


        //print the last 3 keys of the final merged array
        //printf("Master process %d final merged array KEYS: ", my_id);
        //for(size_t i = 0; i < local_A_size; ++i) {
        //    printf("%lu ", local_A[i].key);
        //}
        //printf("\n");
        //printf("Master process %d final merged array PAYLOADS: ", my_id);
        //for(size_t i = 0; i < local_A_size; ++i) {
        //    printf("%s ", local_A[i].payload);
        //}
        //printf("\n");
    
    }

    delete[] local_A;
   
   // printf("Process %d almost finished execution.\n", my_id);

   // MPI_Barrier(MPI_COMM_WORLD); // Ensure all processes reach this point before finalizing
    
   // printf("Process %d passed the barrier.\n", my_id);

    MPI_Finalize();
    //printf("Process %d finished execution.\n", my_id);

    return 0;
}