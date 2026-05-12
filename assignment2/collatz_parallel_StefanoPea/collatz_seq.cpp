#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <hpc_helpers.hpp>


// SEQUENTIAL IMPLEMENTATION OF THE COLLATZ CONJECTURE

using ull = unsigned long long;
ull collatz_length(ull n) {
    ull steps = 0;
    while (n != 1) {
        n = (n % 2 == 0) ? n / 2 : 3 * n + 1;
        ++steps;
    }
    return steps;
}


int max_collatz_in_range(ull start, ull end, ull &tot_length) {
    int max_length = 0;
    for (ull i = start; i <= end; ++i) {
        int length = collatz_length(i);
        tot_length += length;
        if (length > max_length) {
            max_length = length;
        }
    }
    return max_length;
}


// Function to parse input ranges (e.g., "1-1000") and return the start and end values
bool parse_range(const std::string& range, ull& start, ull& end) {
    std::stringstream ss(range);
    std::string token;
    if (std::getline(ss, token, '-')) {
        try {
            start = std::stoull(token);
            if (std::getline(ss, token)) {
                end = std::stoull(token);
                return start <= end; // Valid range
            }
        } catch (...) {
            return false;
        }
    }
    return false;
}



int main(int argc, char* argv[]) {
    
    std::cout << "Sequential implementation\n";

    ull tot_length = 0;

    TIMERSTART(timer_seq_Tot)

    for (int i = 1; i < argc; ++i) {
        ull start, end;
        if (!parse_range(argv[i], start, end)) {
            std::cerr << "Invalid range format: " << argv[i] << "\n";
            continue;
        }

        int max_length = max_collatz_in_range(start, end, tot_length);


        std::cout << argv[i] << ": " << max_length << "\n";
    }
    
    TIMERSTOP(timer_seq_Tot)
    std::cout << "Total Collatz lengths: " << tot_length << "\n";

    return 0;
}