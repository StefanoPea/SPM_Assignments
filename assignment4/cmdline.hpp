#if !defined _CMDLINE_HPP
#define _CMDLINE_HPP

#include <cstdio>
#include <string>

#include <utility.hpp>
#include <bits/getopt_core.h>
#include <cctype> 
#include <cstdlib> 


static size_t RPAYLOAD    = 4;     // default record payload size in bytes
static size_t ARRAYSIZE   = 128;    // size of the array to be sorted (in number of records)
static size_t FFTHREADS   = 16;      // number of FastFlow threads (including emitter and collector)
static size_t MODE        = 0;      // 0 for sequential, 1 for fast flow parallel

// check if the string 's' is a number, otherwise it returns false
static bool isNumber(const char* s, long &n) {
    try {
    size_t e;
    n=std::stol(s, &e, 10);
    return e == strlen(s);
    } catch (const std::invalid_argument&) {
    return false;
    } catch (const std::out_of_range&) {
    return false;
    }
}

static inline char* getOption(char **begin, char **end, const std::string &option) {
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end) return *itr;
    return nullptr;
}

static inline void usage(const char *argv0) {
    std::printf("--------------------\n");
    std::printf("Usage: %s [options] file-or-directory [file-or-directory]\n", argv0);
    std::printf("\nOptions:\n");
    std::printf(" -s N: array size  (e.g., -s 10M -s 100M\n");
    std::printf(" -r R: record payload  (in bytes, e.g., -r 8, -r 64, -r 256)");
    std::printf(" -t T: number of FastFlow threads (e.g., -t 16, -t 32)");
    std::printf(" -m M: mode of execution (e.g. 0 for sequential, 1 for fast flow parallel)");
    std::printf("--------------------\n");
}

int parseCommandLine(int argc, char *argv[]) {
    extern char *optarg;
    const std::string optstr = "s:r:t:m:";
    long opt, start = 1;

    while ((opt = getopt(argc, argv, optstr.c_str())) != -1) {
        switch (opt) {

            case 's': {
                long s = 0;
                if (!isNumber(optarg, s)) {
                    std::fprintf(stderr, "Error: wrong '-s' option\n");
                    usage(argv[0]);
                    return -1;
                }
                ARRAYSIZE = s;
                start += 2;
            } break;
            
            case 'r': {
                long r = 0;
                if (!isNumber(optarg, r)) {
                    std::fprintf(stderr, "Error: wrong '-r' option\n");
                    usage(argv[0]);
                    return -1;
                }
                RPAYLOAD = r;
                start += 2;
            } break;
            
            case 't': {
                long t = 0;
                if (!isNumber(optarg, t)) {
                    std::fprintf(stderr, "Error: wrong '-t' option\n");
                    usage(argv[0]);
                    return -1;
                }
                FFTHREADS = t;
                start += 2;
            } break;

            case 'm': {
                long m = 0;
                if (!isNumber(optarg, m)) {
                    std::fprintf(stderr, "Error: wrong '-m' option\n");
                    usage(argv[0]);
                    return -1;
                }
                MODE = m;
                start += 2;
            } break;
            

            default:
                usage(argv[0]);
                return -1;
        }
    }

    return start;
}

#endif // _CMDLINE_HPP