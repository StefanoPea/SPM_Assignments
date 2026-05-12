#include <cstddef>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdint>
#include "miniz.h"
#include "config.hpp"
#include "cmdline.hpp"
#if !defined _MY_UTILS_HPP
#define _MY_UTILS_HPP

/**
 * Compress a file sequentially.
 * The implementation follows the provided minizSeq code.
 * The only difference with the original code is the presence of 
 * a flag in the header to signify that the file has been 
 * compressed sequentially. 
 *
 * @param[in] ptr       Pointer to the data to be compressed.
 * @param[in] filePath  Path to the file to be compressed.
 * @param[in] size      Size of the data to be compressed.
 * 
 * @return true if compression was successful, false otherwise.
 * 
 */
static inline bool compressSeq(unsigned char *ptr, const std::string& filePath, size_t size);

/**
 * Decompress a file sequentially.
 * Reads the flag+size header, then uncompresses the data
 * writing out the original file name (removing “.zip”).
 *
 * @param[in] ptr      Pointer to the compressed data.
 * @param[in] filePath Path to the “.zip” file.
 * @param[in] size     Size of the compressed buffer.
 * 
 * @return true if decompression succeeded, false otherwise.
 * 
 */
static inline bool decompressSeq(unsigned char *ptr, const std::string& filePath, size_t size);

/**
 * Compress a file in parallel, chunked mode.
 * Splits the input into fixed‐size blocks, compresses each
 * block in its own OpenMP thread, and writes a header:
 * flag=1, origSize, chunkCount, chunk sizes, chunked data.
 *
 * @param[in] inPtr    Pointer to the data to compress.
 * @param[in] filePath Path to the file to be written.
 * @param[in] size     Size of the data to compress.
 * 
 * @return true if all chunks compressed and file written, false otherwise.
 * 
 */
static inline bool compressPar(unsigned char *ptr, const std::string& filePath, size_t size);

/**
 * Decompress a file in parallel, chunked mode.
 * Parses the header (flag=1, origSize, chunkCount, sizes…),
 * then spawns threads to uncompress each chunk, 
 * then removes the “.zip”.
 *
 * @param[in] ptr      Pointer to the compressed buffer.
 * @param[in] filePath Path to the “.zip” file.
 * @param[in] size     Size of the compressed buffer.
 * 
 * @return true if all chunks decompressed and file written, false otherwise.
 *
 */
static inline bool decompressPar(unsigned char *ptr, const std::string& filePath, size_t size);

/**
 * Process a single path: either compress or decompress it in parallel.
 * Calls compressSeq/decompressSeq for small files, or 
 * compressPar/decompressPar for large files. 
 * Chooses based on COMP flag and by inspecting 
 * the header flag in the mapped input (in case of decompression).
 *
 * @param[in] filePath Path of file to process.
 * @param[in] size     Size of the file.
 * @param[in] comp     If true, compress; if false, decompress.
 * 
 * @return true if the operation succeeded, false on any error.
 */
static inline bool doWorkPar(const char filePath[], size_t size, const bool comp);

//-----------------------------------------------------------------------------------------------------------

static inline bool compressSeq(unsigned char *ptr, const std::string& filePath, size_t size) {
    
    unsigned char * inPtr  = ptr;
	size_t          inSize = size;

    
    // Compress the data
    size_t cmp_len = compressBound(size);
    unsigned char *ptrOut = new unsigned char[cmp_len];



    if (compress(ptrOut, &cmp_len, (const unsigned char *)inPtr, inSize) != Z_OK) {
		if (QUITE_MODE>=1) 
			std::fprintf(stderr, "Failed to compress file in memory\n");    
		delete [] ptrOut;
		return false;
	}

    // Write the compressed data to a new file
    std::string outPath = filePath + ".zip";
    std::ofstream out(outPath, std::ios::binary);

    if (!out.is_open()) {
		std::fprintf(stderr, "Failed to open output file: %s\n", outPath.c_str());
		delete [] ptrOut;
		return false;
	}

    // Compression header for sequential files

    // 0 for sequential, 1 for parallel
    uint8_t flag = 0;                                               
    out.write(reinterpret_cast<char*>(&flag), sizeof(flag));
    
    // original size of the file
    out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    
    // compressed data
    out.write(reinterpret_cast<const char*>(ptrOut), cmp_len);
    
    out.close();


    if (REMOVE_ORIGIN) {
		unlink(filePath.c_str());
	}
	
	delete[] ptrOut;
	out.close();


    return true;
}


//-----------------------------------------------------------------------------------------------------------

static inline bool decompressSeq(unsigned char* ptr, const std::string& filePath, size_t size){
    
    // Read the format flag:
    uint8_t flag = *ptr;
    if (flag != 0) {
        std::cerr << "Input not in expected sequential format\n";
        return false;
    }

    // Read original size from the bytes immediately after the flag
    ptr += 1;
    size -= 1;
    size_t decompressedSize = *reinterpret_cast<size_t*>(ptr);

    // Advance past the size header
    ptr  += sizeof(size_t);
    size -= sizeof(size_t);

    unsigned char* decompressed_data = nullptr;
    std::string outfile = filePath.substr(0, filePath.size() - 4);

    if (!allocateFile(outfile.c_str(), decompressedSize, decompressed_data))
    return false;

    if (uncompress(decompressed_data, &decompressedSize, ptr, size)!= Z_OK){
        if (QUITE_MODE >= 1)
            std::fprintf(stderr, "uncompress failed!\n");
        unmapFile(decompressed_data, decompressedSize);
        return false;
    }

    unmapFile(decompressed_data, decompressedSize);

    if (REMOVE_ORIGIN) {
        unlink(filePath.c_str());
    }

    return true;
}


//-----------------------------------------------------------------------------------------------------------

static inline bool compressPar(unsigned char *inPtr, const std::string &filePath, size_t size){

    // Calculate number of chunks (round up)
    uint32_t numChunks = static_cast<uint32_t>((size + CHUNK_SIZE - 1) / CHUNK_SIZE);

    // Prepare arrays for each chunk’s compressed size & buffer
    std::vector<uint32_t> compSizes(numChunks);
    std::vector<unsigned char*> compChunks(numChunks);

    // Allocate each chunk's worst-case buffer
    size_t cmp_len = compressBound(CHUNK_SIZE);
    for (uint32_t i = 0; i < numChunks; ++i) {
        compChunks[i] = new unsigned char[cmp_len];
    }

    // Parallel compress each chunk
    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(numChunks); ++i) {
        size_t offset   = static_cast<size_t>(i) * CHUNK_SIZE;
        size_t thisSize = std::min(CHUNK_SIZE, size - offset);
        unsigned char * chunkIn = inPtr + offset;

        size_t outLen = cmp_len;
        int status = compress(compChunks[i], &outLen, chunkIn, thisSize);
        if (status != Z_OK) {
            if (QUITE_MODE >= 1)
            std::fprintf(stderr,"compress failed on chunk %d (status=%d)\n", i, status);

            outLen = 0;
        }
        compSizes[i] = static_cast<uint32_t>(outLen);
    }

    // Write everything out
    std::string outPath = filePath + ".zip";
    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
        std::fprintf(stderr, "Failed to open output file: %s\n",
        outPath.c_str());
        // clean up
        for (auto ptr : compChunks) delete[] ptr;
        return false;
    }

    // Parallel header: flag=1, origSize, chunkCount, per-chunk sizes
    uint8_t  flag = 1;                
    out.write(reinterpret_cast<char*>(&flag), 1);
    uint64_t orig = static_cast<uint64_t>(size);
    out.write(reinterpret_cast<char*>(&orig), sizeof(orig));
    out.write(reinterpret_cast<char*>(&numChunks), sizeof(numChunks));
    for (auto s : compSizes) {
        uint32_t sz = s;
        out.write(reinterpret_cast<char*>(&sz), sizeof(sz));
    }

    // Chunk data
    for (uint32_t i = 0; i < numChunks; ++i) {
        if (compSizes[i] > 0) {
            out.write(reinterpret_cast<char*>(compChunks[i]),
            compSizes[i]);
        }
        delete[] compChunks[i];
    }


    if (REMOVE_ORIGIN) {
        unlink(filePath.c_str());
    }

    out.close();

    return true;
}


//-----------------------------------------------------------------------------------------------------------

static inline bool decompressPar(unsigned char* ptr, const std::string& filePath, size_t size){
    
    // Read the format flag
    uint8_t flag = *ptr++;
    if (flag != 1) {
        std::cerr << "Input not in expected parallel format\n";
        return false;
    }
    size -= 1;

    // Read original uncompressed size
    uint64_t origSize = *reinterpret_cast<uint64_t*>(ptr);
    ptr  += sizeof(origSize);
    size -= sizeof(origSize);

    // Read number of chunks
    uint32_t chunkCount = *reinterpret_cast<uint32_t*>(ptr);
    ptr  += sizeof(chunkCount);
    size -= sizeof(chunkCount);

    // Read each compressed‐chunk size
    std::vector<uint32_t> compSizes(chunkCount);
    for (uint32_t i = 0; i < chunkCount; ++i) {
        compSizes[i] = *reinterpret_cast<uint32_t*>(ptr);
        ptr  += sizeof(uint32_t);
        size -= sizeof(uint32_t);
    }

    std::vector<size_t> compOffsets(chunkCount);
    
    size_t off = 0;
    for (uint32_t i = 0; i < chunkCount; ++i) {
        compOffsets[i] = off;
        off += compSizes[i];
    }
    

    // Memory-map the output file
    unsigned char* outBuf = nullptr;
    std::string    outPath = filePath.substr(0, filePath.find_last_of('.'));
    if (!allocateFile(outPath.c_str(), origSize, outBuf)) {
        std::cerr << "Failed to allocate output file\n";
        return false;
    }

    // Decompress each chunk in parallel
    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(chunkCount); ++i) {
        
        size_t thisSize = CHUNK_SIZE;
        if (static_cast<uint32_t>(i) == chunkCount - 1) {
            thisSize = origSize - static_cast<size_t>(CHUNK_SIZE) * (chunkCount - 1);
        }

        // Setup pointers into our single big input buffer:
        const unsigned char* compPtr = ptr + compOffsets[i];
        mz_ulong             destLen = thisSize;

        int status = uncompress(outBuf + static_cast<size_t>(i) * CHUNK_SIZE, &destLen, compPtr, compSizes[i]);
        if (status != Z_OK) {
            if (QUITE_MODE >= 1) {
                std::fprintf(stderr, "uncompress failed on chunk %d (status=%d)\n",
                i, status);
            }
        }
    }

   
    unmapFile(outBuf, origSize);

    if (REMOVE_ORIGIN) {
        unlink(filePath.c_str());
    }

    return true;
}


//-----------------------------------------------------------------------------------------------------------

static inline bool doWorkPar(const char filePath[], size_t size, const bool comp) {
    
    unsigned char *ptr = nullptr;
    if (!mapFile(filePath, size, ptr)) {
		if (QUITE_MODE>=1) 
			std::fprintf(stderr, "mapFile %s failed\n", filePath);
		return false;
	}
    // if the file needs to be compressed, just check the file size to decide which compression policy to use
    if (comp)
    {
        if (size < CHUNK_SIZE) {
            // use sequential compression for small files
            return compressSeq(ptr, filePath, size);
        } else {
            // use parallel compression for large files
            return compressPar(ptr, filePath, size);
        }
    }
    else
    {
        // if the file needs to be decompressed, check the header to decide which decompression policy to use
        uint8_t flag = ptr[0];

        // check the flag to determine the compression method
        // 0 for sequential, 1 for parallel
        if (flag == 0) {
            return decompressSeq(ptr,filePath, size);
        } else if (flag == 1) {
            return decompressPar(ptr, filePath, size);
        } else {
            std::cerr << "Input not in expected format\n";
            return false;
        }
    }    
    
    unmapFile(ptr, size);

    
    return true;    


}




#endif // _MY_UTILS_HPP
