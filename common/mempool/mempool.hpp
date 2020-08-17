/*
    Socle - Socket Library Ecosystem
    Copyright (c) 2014, Ales Stibal <astib@mag0.net>, All rights reserved.

    This library  is free  software;  you can redistribute  it and/or
    modify  it  under   the  terms of the  GNU Lesser  General Public
    License  as published by  the   Free Software Foundation;  either
    version 3.0 of the License, or (at your option) any later version.
    This library is  distributed  in the hope that  it will be useful,
    but WITHOUT ANY WARRANTY;  without  even  the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the GNU Lesser General Public License for more details.

    You  should have received a copy of the GNU Lesser General Public
    License along with this library.
*/

#ifndef __MEMPOOL_HPP__
#define __MEMPOOL_HPP__

#include <cstddef>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <atomic>

#include <execinfo.h>

#include <display.hpp>
#include <log/logger.hpp>

#define MEMPOOL_DEBUG

class buffer;

typedef struct mem_chunk
{
    mem_chunk(): ptr(nullptr), capacity(0) {};
    explicit mem_chunk(std::size_t s): capacity(s) { ptr = new unsigned char[s]; };
    mem_chunk(unsigned char* p, std::size_t c): ptr(p), capacity(c) {};

    // Actually coverity found wanted feature - mem_chunk is basically pointer with size
    // and should be treated as a copyable *value*.

    // this is for study purposes, but don't use it in production.
    //~mem_chunk () { delete ptr; };   // coverity: 1407975
    //mem_chunk(mem_chunk const& ref) = delete;
    //mem_chunk& operator=(mem_chunk const& ref) = delete;

    unsigned char* ptr;
    std::size_t  capacity;
    bool in_pool = false; // set this flag to indicate if the allocation is in pool => allocated, but not used.

    enum class pool_type_t { POOL, HEAP };
    using type = pool_type_t;
    type pool_type = type::POOL;

#ifdef MEMPOOL_DEBUG
    static inline const bool trace_enabled = true;
#else
    static inline const bool trace_enabled = false;
#endif



#ifdef MEMPOOL_DEBUG

    #define MEM_CHUNK_TRACE_SZ 64

    void* trace[MEM_CHUNK_TRACE_SZ];
    int trace_size = 0;                 // number of elements (void* pointers) in the trace list
    uint32_t mark = 0;                  // useful for filtering purposes.

    inline void clear_trace() { memset(trace, 0 , MEM_CHUNK_TRACE_SZ*sizeof(void*)); trace_size = 0; mark = 0; }
    inline void set_trace() { clear_trace(); trace_size = backtrace(trace, MEM_CHUNK_TRACE_SZ); };
    #ifndef LIBC_MUSL
    std::string str_trace() {

        std::string ret;
        char **strings;

        strings = backtrace_symbols( trace, trace_size );

        if (strings == nullptr) {
            ret += "failure: backtrace_symbols";
            return  ret;
        }


        for( int i = 0; i < trace_size; i++ ) {
            ret += "\n";
            ret += strings[i];
        }
        delete[] strings;

        return ret;
    };
    std::string simple_trace() {
        std::string ret;
        for( int i = 0; i < trace_size; i++ ) {
            ret += string_format("0x%x ", trace[i]);
        }
        return ret;
    };

    #else //LIBC_MUSL
    std:string str_trace() {
        return simple_trace();
    };
    #endif
#else
    inline void clear_trace() {};
    inline void set_trace() {};
#endif

} mem_chunk_t;


class memPool {

    std::size_t sz32;
    std::size_t sz64;
    std::size_t sz128;
    std::size_t sz256;
    std::size_t sz1k;
    std::size_t sz5k;
    std::size_t sz10k;
    std::size_t sz20k;
    std::size_t sz35k;
    std::size_t sz50k;

    std::vector<mem_chunk_t>* pick_acq_set(ssize_t s);
    std::vector<mem_chunk_t>* pick_ret_set(ssize_t s);

    std::vector<mem_chunk_t> available_32;
    std::vector<mem_chunk_t> available_64;
    std::vector<mem_chunk_t> available_128;
    std::vector<mem_chunk_t> available_256;
    std::vector<mem_chunk_t> available_1k;
    std::vector<mem_chunk_t> available_5k;
    std::vector<mem_chunk_t> available_10k;
    std::vector<mem_chunk_t> available_20k;
    std::vector<mem_chunk_t> available_35k;
    std::vector<mem_chunk_t> available_50k;
//    std::vector<mem_chunk_t> available_big; // will be empty initially

    memPool(std::size_t sz256, std::size_t sz1k, std::size_t sz5k, std::size_t sz10k, std::size_t sz20k);
    ~memPool() noexcept {

        auto chunk_deleter = [](mem_chunk& m) { delete[] m.ptr; m.ptr = nullptr; };
        auto pool_deleter = [](auto& pool, auto chunk_deleter) {
            std::for_each(pool.begin(), pool.end(), chunk_deleter );
            pool.clear();
        };

        try {

            std::lock_guard<std::mutex> g(lock);

            pool_deleter(available_32, chunk_deleter);
            pool_deleter(available_64, chunk_deleter);
            pool_deleter(available_128, chunk_deleter);
            pool_deleter(available_256, chunk_deleter);
            pool_deleter(available_5k, chunk_deleter);
            pool_deleter(available_10k, chunk_deleter);
            pool_deleter(available_20k, chunk_deleter);
            pool_deleter(available_35k, chunk_deleter);
            pool_deleter(available_50k, chunk_deleter);
        } catch (std::exception const& e) {
            std::cerr << "exception in ~memPool(): " <<  e.what() << std::endl;
        }
    }

public:

    // indicate to not use any allocation functions which are not safe!
    // resource requests will fail and releases do nothing.
    std::atomic_bool bailing = false;

    static memPool& pool() {
        static memPool m = memPool(5000,10000,10000,1000,800);
        return m;
    }

    void extend(std::size_t sz256, std::size_t sz1k, std::size_t sz5k, std::size_t sz10k, std::size_t sz20k);

    mem_chunk_t acquire(std::size_t sz);
    void release(mem_chunk_t to_ret);

    std::atomic<unsigned long long> stat_acq;
    std::atomic<unsigned long long> stat_acq_size;

    std::atomic<unsigned long long> stat_ret;
    std::atomic<unsigned long long> stat_ret_size;

    std::atomic<unsigned long long> stat_alloc;
    std::atomic<unsigned long long> stat_alloc_size;

    std::atomic<unsigned long long> stat_free;
    std::atomic<unsigned long long> stat_free_size;

    std::atomic<unsigned long long> stat_out_free;
    std::atomic<unsigned long long> stat_out_free_size;

    std::atomic<unsigned long long> stat_out_pool_miss;
    std::atomic<unsigned long long> stat_out_pool_miss_size;


    long unsigned int mem_32_av() const { return static_cast<long unsigned int>(available_32.size()); };
    long unsigned int mem_64_av() const { return static_cast<long unsigned int>(available_64.size()); };
    long unsigned int mem_128_av() const { return static_cast<long unsigned int>(available_128.size()); };
    long unsigned int mem_256_av() const { return static_cast<long unsigned int>(available_256.size()); };
    long unsigned int mem_1k_av() const { return static_cast<long unsigned int>(available_1k.size()); };
    long unsigned int mem_5k_av() const { return static_cast<long unsigned int>(available_5k.size()); };
    long unsigned int mem_10k_av() const { return static_cast<long unsigned int>(available_10k.size()); };
    long unsigned int mem_20k_av() const { return static_cast<long unsigned int>(available_20k.size()); };
    long unsigned int mem_35k_av() const { return static_cast<long unsigned int>(available_35k.size()); };
    long unsigned int mem_50k_av() const { return static_cast<long unsigned int>(available_50k.size()); };
//    long unsigned int mem_big_av() const { return static_cast<long unsigned int>(available_big.size()); };


    long unsigned int mem_32_sz() const { return static_cast<long unsigned int>(sz32); };
    long unsigned int mem_64_sz() const { return static_cast<long unsigned int>(sz64); };
    long unsigned int mem_128_sz() const { return static_cast<long unsigned int>(sz128); };
    long unsigned int mem_256_sz() const { return static_cast<long unsigned int>(sz256); };
    long unsigned int mem_1k_sz() const { return static_cast<long unsigned int>(sz1k); };
    long unsigned int mem_5k_sz() const { return static_cast<long unsigned int>(sz5k); };
    long unsigned int mem_10k_sz() const { return static_cast<long unsigned int>(sz10k); };
    long unsigned int mem_20k_sz() const { return static_cast<long unsigned int>(sz20k); };
    long unsigned int mem_35k_sz() const { return static_cast<long unsigned int>(sz35k); };
    long unsigned int mem_50k_sz() const { return static_cast<long unsigned int>(sz50k); };


    std::mutex lock;
};


// hashmap of pointer sizes (for mempool_* functions)
//

struct mpdata {

    static std::unordered_map<void *, mem_chunk>& map() {
        static std::unordered_map<void *, mem_chunk> m;
        return m;
    }

    static std::mutex& lock() {
        static std::mutex m;
        return m;
    };

    #ifdef MEMPOOL_DEBUG
    static std::unordered_map<void *, mem_chunk>& trace_map() {
        static std::unordered_map<void *, mem_chunk> m;
        return m;
    }
    static std::mutex& trace_lock() {
        static std::mutex m;
        return m;
    };

    #endif

};

class mempool_bad_alloc : public std::runtime_error {
public:
    int block_size {0};
    mempool_bad_alloc(const char* e) : std::runtime_error(e) {};
    mempool_bad_alloc(const char* e, int block_size) : std::runtime_error(e), block_size(block_size) {};
};

void* mempool_alloc(size_t);
void* mempool_realloc(void*, size_t);
void  mempool_free(void*);


// wrapper functions for compatibility with openssl malloc hooks (see CRYPTO_set_mem_functions)
void* mempool_alloc(size_t, const char*, int);
void* mempool_realloc(void*, size_t, const char*, int);
void mempool_free(void*, const char*, int);


struct mp_stats {

    std::atomic<unsigned long long> stat_mempool_alloc;

    std::atomic<unsigned long long> stat_mempool_realloc;
    std::atomic<unsigned long long> stat_mempool_realloc_miss;
    std::atomic<unsigned long long> stat_mempool_realloc_fitting;

    std::atomic<unsigned long long> stat_mempool_free;
    std::atomic<unsigned long long> stat_mempool_free_miss;

    std::atomic<unsigned long long> stat_mempool_alloc_size;
    std::atomic<unsigned long long> stat_mempool_realloc_size;
    std::atomic<unsigned long long> stat_mempool_free_size;

    static mp_stats& get() { static mp_stats m; return m; }

    mp_stats(mp_stats const&) = delete;
    mp_stats& operator=(mp_stats const&) = delete;
private:
    mp_stats() = default;
};

#endif //__MEMPOOL_HPP__