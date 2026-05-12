#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <mutex>
#include <future>
#include <hpc_helpers.hpp>
#include <threadPool.hpp>


// PARALLEL IMPLEMENTATION OF THE COLLATZ CONJECTURE


using ull = unsigned long long;

// Struct to store the input args
struct InputArgs {
    bool dynamic_scheduling = false;
    int num_threads = 16;
    int chunk_size = 1;
    std::vector<std::pair<ull, ull>> ranges;
};

// Collatz sequence computation
ull collatz_length(ull n) {
    ull steps = 0;
    while (n != 1) {
        n = (n % 2 == 0) ? n / 2 : 3 * n + 1;
        ++steps;
    }
    return steps;
}

// Function to parse command line arguments
InputArgs parse_arguments(int argc, char* argv[]) {
    InputArgs args;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-d") {
            args.dynamic_scheduling = true;
        } 
        else if (arg == "-n" && i + 1 < argc) {
            args.num_threads = std::stoi(argv[++i]);
        } 
        else if (arg == "-c" && i + 1 < argc) {
            args.chunk_size = std::stoi(argv[++i]);
        } 
        else {
            size_t dash_pos = arg.find('-');
            if (dash_pos != std::string::npos) {
                ull start = std::stoull(arg.substr(0, dash_pos));
                ull end = std::stoull(arg.substr(dash_pos + 1));
                args.ranges.emplace_back(start, end);
            }
        }
    }

    return args;
}


int main(int argc, char* argv[]) {

    InputArgs args = parse_arguments(argc, argv);

    std::cout << (args.dynamic_scheduling ? "Dynamic scheduling.\n" : "Static scheduling.\n");
    std::cout << "Running with " << args.num_threads << " threads and a chunk size of "
              << args.chunk_size << ".\n";

    // Start the timer
    TIMERSTART(timer_par_Tot);

    // Variable to store the maximum Collatz length
    // it does not need to be thread-safe because it is accessed only by the main thread after all threads have finished
    ull global_max = 0;

    if (args.dynamic_scheduling) {

        // Dynamic scheduling using a thread pool
        ThreadPool TP(args.num_threads);

        // We treat each range separately
        for (const auto& range : args.ranges) {

            // Vector to store the futures returned by the worker threads
            // each future will contain the maximum Collatz length for a chunk
            std::vector<std::future<ull>> futures;

            for (ull i = range.first; i <= range.second; i += args.chunk_size) {
                ull chunk_end = std::min(i + args.chunk_size - 1, range.second); // upper bound of the block
                
                // enque the task to the threadpool using a lambda function
                futures.emplace_back(TP.enqueue([i, chunk_end]() -> ull {
                    
                
                    ull local_max = 0; // variable returning the maximum Collatz length for this  chunk
                    for (ull j = i; j <= chunk_end; ++j) {
                        local_max = std::max(local_max, collatz_length(j));
                    }

                    return local_max;
                
                }));


            }
            // Cycle through the futures and get the maximum Collatz length for this range
            for (auto& fut : futures) {
                global_max = std::max(global_max, fut.get());
            }

            global_max = 0; // reset the global maximum for the next range

        }


    } else {


        // Static scheduling using threads
        // The threads are created and waited for the termination for each range, introducing some small overhead
        for (const auto& range : args.ranges) {
    

            ull start = range.first, end = range.second;
            std::vector<std::thread> threads;

            // in this case we create a vector of local maxima, each thread will access its own index, determined by the thread id
            // the safety of this operation is guaranteed by the fact that each thread will only access its own index
            std::vector<ull> local_max_values(args.num_threads, 0);

            for (int t = 0; t < args.num_threads; t++) {

                // lambda function to be executed by each thread
                // each thread will process a chunk of the range, starting from its own index
                threads.emplace_back([start, end, chunk_size = args.chunk_size, t, num_threads = args.num_threads, &local_max_values]() {
                    ull local_max = 0;
                    for (ull i = start + t * chunk_size; i <= end; i += num_threads * chunk_size) {
                        ull chunk_end = std::min(i + chunk_size - 1, end); // for when the chunk is less than the standard dimension
                        for (ull j = i; j <= chunk_end; ++j) {
                            local_max = std::max(local_max, collatz_length(j));
                        }
                    }
                    // we store the maximum Collatz length for this chunk in the vector of local maxima at the index of the thread id
                    // this is safe because each thread will only access its own index
                    local_max_values[t] = local_max;

                });
            }

            // wait for the termination of all the threads
            for (auto& t : threads) t.join();

            // get the maximum Collatz length for this range from the vector of local maxima
            ull max_steps = *std::max_element(local_max_values.begin(), local_max_values.end());
            global_max = std::max(global_max, max_steps);
        }
    }

    // stop the timer
    TIMERSTOP(timer_par_Tot);

    return 0;
}
