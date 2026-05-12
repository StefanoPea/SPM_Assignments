#if !defined _UTILITY_HPP
#define _UTILITY_HPP

#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <cmdline.hpp>

using namespace ff;

// Struct for the records of the array to be sorted
struct Record {
  
    unsigned long key;

    char* payload = nullptr;

    Record() : key{0}, payload{nullptr}{}
    Record(const Record &o): key{o.key}, payload{o.payload}{}
    Record & operator= (const Record &o) {
        key = o.key;
        payload = o.payload;
        return *this;
    }

    ~Record() {}

 
};
bool operator<(Record const&a, Record const&b){ return a.key<b.key; }


// A task for the FastFlow farm, representing a sort or a merge operation depending on level
struct Task {
    Record* data;
    size_t lo1, hi1;  // first run positions = [lo1,hi1)
    size_t lo2, hi2;  // second run positions = [lo2,hi2)
    size_t chunkpos1, chunkpos2; // position of the chunks in the original array, used for merging runs
    size_t level; // level of the task in the merge sort
    size_t size; // size of the task (number of records in the run)

    Task() : data(nullptr), lo1(0), hi1(0), lo2(0), hi2(0), chunkpos1(0), chunkpos2(0), level(0) {}

    // Parameterized constructor
    Task(Record* d, size_t l1, size_t h1, size_t l2, size_t h2, size_t cp1, size_t cp2, size_t lvl)
        : data(d), lo1(l1), hi1(h1), lo2(l2), hi2(h2), chunkpos1(cp1), chunkpos2(cp2), level(lvl) {}

};
  bool operator<(Task const&a, Task const&b) {
    return a.chunkpos1 < b.chunkpos1 || (a.chunkpos1 == b.chunkpos1 && a.chunkpos2 < b.chunkpos2);
}

/**
 * @brief Check if an array of Records is sorted
 * 
 * This function checks if the array of Records is sorted based on the key.
 * 
 * @param A Pointer to the array of Records to be checked
 * @param n Size of the array
 * 
 * @return true if the array is sorted, false otherwise
 */
bool is_sorted(Record* A, size_t n){

    bool sorted = true;
    for (size_t i = 1; i < n; ++i) {
        if (A[i - 1].key > A[i].key) {
            sorted = false;
            break;
        }
    }
    return sorted;
}


/**
 * @brief Sequential merge sort implementation
 * 
 * This function sorts the array of Records using a sequential implementation of merge sort.
 * This will also be used by the FastFlow farm workers to sort initial chunks.
 * 
 * @param A Pointer to the array of Records to be sorted
 * @param n Size of the array
 */
void seqMergeSort(Record* A, size_t n) {
  if (n<2) return;
  size_t m = n/2;
  seqMergeSort(A, m);
  seqMergeSort(A+m, n-m);
  std::inplace_merge(A, A+m, A+n);
}


/**
 * @brief Merges two sorted chunks into a target array
 * 
 * This function merges a source chunk into a target array, resizing the target
 * array as necessary. It uses std::merge to perform the merge operation.
 * 
 * @param targetArray Pointer to the target array where the merged result will be stored
 * @param targetSize Reference to the size of the target array (will be updated)
 * @param sourceChunk Pointer to the source chunk to be merged
 * @param sourceChunkSize Size of the source chunk
 * @param records_allocated_from_the_other_side Optional pointer to an array allocated on the other side (default is nullptr)
 */
void mergeSortedChunks(Record*& targetArray, size_t& targetSize, Record* sourceChunk, size_t sourceChunkSize, Record* records_allocated_from_the_other_side = nullptr) {
    
    if (records_allocated_from_the_other_side == nullptr) {
        records_allocated_from_the_other_side = new Record[targetSize + sourceChunkSize];
    }

    std::merge(targetArray, &(targetArray[targetSize]), sourceChunk, &(sourceChunk[sourceChunkSize]), records_allocated_from_the_other_side);
    
    //cleanup
    delete[] targetArray;
    delete[] sourceChunk;
    
    targetArray = records_allocated_from_the_other_side;
    targetSize = targetSize + sourceChunkSize;
    
    return;

}


/**
 * @brief Worker node for the FastFlow farm
 * 
 * This block handles the sequential sorting and merging of tasks.
 */
class Worker : public ff_node {
public:
  void* svc(void* t) override {
    //termination
    if (t == EOS) return EOS;

    Task* task = static_cast<Task*>(t);

    if (task->level == 0) { // Initial sort task from Emitter
    
        task->level = 1;    // Promote to level 1 after sorting        
        seqMergeSort(task->data + task->lo1, task->hi1 - task->lo1);
        ff_send_out(task);     
        return GO_ON;
      
    } else { // Merge task from Collector
          

        std::inplace_merge(task->data + task->lo1, task->data + task->lo2, task->data + task->hi2);
        task->hi1 = task->hi2; // hi1 now marks the end of the new merged run
        task->lo2 = task->lo1; // lo2 can be set to lo1 for consistency of a single run
        task->level++; 
        ff_send_out(task);
        return GO_ON;
    }
  }  
};


/**
 * @brief Master node for the FastFlow farm (Emitter + Collector)
 * 
 * This block handles the initial task distribution and the distribution of 
 * runs to be merged together.
 */
class Master : public ff_monode_t<Task, Task> {
    private:
        
        Record* _A;         // Pointer to the original data array
        size_t _N;          // Total number of records
        unsigned _W;        // Number of worker threads (used to determine initial chunking)
    
        bool initial_tasks_sent;
        unsigned actual_num_chunks; // Actual number of initial chunks generated
    
        // Stores pending sorted runs. Key: chunkpos1 (start index of original chunks)
        std::map<size_t, Task*> pending_runs;
        bool job_completely_done; // Flag for termination
    
    public:
        Master(Record* A_arr, size_t N_records, unsigned num_workers)
            : _A(A_arr), _N(N_records), _W(num_workers),
              initial_tasks_sent(false), actual_num_chunks(0),
              job_completely_done(false) {}
    
        ~Master() {
            for (auto const& [chunk_start_idx, task_ptr] : pending_runs) {
                delete task_ptr;
            }
            pending_runs.clear();
        }
    
        Task* svc(Task* incoming_run) override { // `incoming_run` is from a Worker, or nullptr on first call
            if (job_completely_done) {
                return EOS; // If already done, just propagate EOS
            }
    
            //prepare initial tasks
            if (!initial_tasks_sent) {
                
                size_t chunk_size = (_N + _W - 1) / _W; // Calculate chunk size
    
                for (unsigned i = 0; i < _W; ++i) {
                    size_t lo = i * chunk_size;
                    size_t hi = std::min(_N, lo + chunk_size);
    
                    if (lo >= hi) continue; // Skip empty chunks
    
                    size_t chunk_idx = actual_num_chunks++; 
                    Task* sort_task = new Task(_A, lo, hi, lo, hi, chunk_idx, chunk_idx, 0);
                    ff_send_out(sort_task);
                }

                initial_tasks_sent = true;        
                return GO_ON; 
            }
    
            if (incoming_run == EOS) {
                job_completely_done = true;
                return EOS;
            }
            
            // This is a sorted run (either initial or merged) from a Worker
            Task* current_run = incoming_run;
    
            // Check for final condition: one run covering all original chunks
            if (actual_num_chunks > 0 && 
                current_run->chunkpos1 == 0 &&
                (current_run->chunkpos2 + 1) == actual_num_chunks) {
                
                
                // The entire array is sorted. Clean up and signal completion.
                delete current_run;       
                job_completely_done = true;
                return EOS;               
            }
            

            // Try to find a partner for current_run to merge with
            Task* partner_run = nullptr;
            bool current_is_left_of_partner = false; 
    
            // Check if current_run can be a RIGHT partner to an existing pending run
            for (auto it = pending_runs.begin(); it != pending_runs.end(); ++it) {
                if (it->second->chunkpos2 + 1 == current_run->chunkpos1) {
                    partner_run = it->second;   // Found a left partner for current_run
                    pending_runs.erase(it);     // Remove partner from map
                    break;
                }
            }
    
            // If no left partner found, check if current_run can be a LEFT partner
            if (!partner_run) {
                auto it = pending_runs.find(current_run->chunkpos2 + 1);
                if (it != pending_runs.end()) {
                    partner_run = it->second;   // Found a right partner for current_run
                    pending_runs.erase(it);     // Remove partner from map
                    current_is_left_of_partner = true;
                }
            }
    
            if (partner_run) { // We found a pair to merge
                
                Task* left_half  = current_is_left_of_partner ? current_run : partner_run;
                Task* right_half = current_is_left_of_partner ? partner_run : current_run;
    
                Task* merge_task = new Task();
                merge_task->data = left_half->data; // Common data array
    
                // Define the two runs for the merge operation
                merge_task->lo1 = left_half->lo1;
                merge_task->hi1 = left_half->hi1;  
                merge_task->lo2 = right_half->lo1; 
                merge_task->hi2 = right_half->hi2;
    
                // The new merged run will span the original chunks of both halves
                merge_task->chunkpos1 = left_half->chunkpos1;
                merge_task->chunkpos2 = right_half->chunkpos2;
                merge_task->level = std::max(left_half->level, right_half->level) + 1;
    
                
                delete left_half;  
                delete right_half;
                
                ff_send_out(merge_task); 
                return GO_ON; 
            
            } else {
                pending_runs[current_run->chunkpos1] = current_run;
                return GO_ON; 
            }
        }
    
        void svc_end() override {
            broadcast_task(EOS);
        }
    };
    


/**
 * @brief FastFlow merge sort implementation
 * 
 * @param chunk_to_sort Pointer to the array of records to be sorted
 * @param chunk_size Size of the array to be sorted
 * @param W_ff_threads Number of worker threads to use in the FastFlow farm
 * @param ff_time Pointer to a float to store the FastFlow execution time  
 */ 
 void sort_chunk_with_fastflow(Record* chunk_to_sort, size_t chunk_size, unsigned W_ff_threads, float* ff_time = nullptr) {
    
    unsigned W_farm = W_ff_threads;
    if (chunk_size < W_farm) { // Don't use more threads than elements
        W_farm = chunk_size;
    }
    if (W_farm == 0 && chunk_size > 0) W_farm = 1; // Ensure at least one worker

    Master master_node(chunk_to_sort, chunk_size, W_farm);
    std::vector<ff_node*> ff_workers;
    for(unsigned i = 0; i < W_farm; ++i) {
        ff_workers.push_back(new Worker);
    }
    
    ff_farm farm;
    farm.add_emitter(&master_node);
    farm.add_workers(ff_workers);
    farm.wrap_around();
    farm.run_and_wait_end();
    if (ff_time) {
        *ff_time = farm.ffTime();
    }

    for(ff_node* worker_ptr : ff_workers) {
        delete worker_ptr;
    }
    ff_workers.clear();
}
    
#endif  // _UTILITY_HPP