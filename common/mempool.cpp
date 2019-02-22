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

#include <mempool.hpp>

#include <unordered_map>
#include "buffer.hpp"

unsigned long long memPool::stat_acq = 0;
unsigned long long memPool::stat_acq_size = 0;

unsigned long long memPool::stat_ret = 0;
unsigned long long memPool::stat_ret_size = 0;

unsigned long long memPool::stat_alloc = 0;
unsigned long long memPool::stat_alloc_size = 0;

unsigned long long memPool::stat_free = 0;
unsigned long long memPool::stat_free_size = 0;

unsigned long long memPool::stat_out_free = 0;
unsigned long long memPool::stat_out_free_size = 0;

unsigned long long stat_mempool_alloc = 0;
unsigned long long stat_mempool_realloc = 0;
unsigned long long stat_mempool_realloc_miss = 0;
unsigned long long stat_mempool_realloc_fitting = 0;
unsigned long long stat_mempool_free = 0;
unsigned long long stat_mempool_free_miss = 0;

unsigned long long stat_mempool_alloc_size = 0;
unsigned long long stat_mempool_realloc_size = 0;
unsigned long long stat_mempool_free_size = 0;

std::unordered_map<void*, size_t> mempool_ptr_map;
std::mutex mempool_ptr_map_lock;


memPool::memPool(std::size_t sz256, std::size_t sz1k, std::size_t sz5k, std::size_t sz10k, std::size_t sz20k):
sz256(sz256), sz1k(sz1k), sz5k(sz5k), sz10k(sz10k), sz20k(sz20k) {

    for(unsigned int i = 0; i < sz256 ; i++) {
        for (int j = 0; j < 10 ; j++) available_32.push_back( { new unsigned char [32], 32 } );
        available_64.push_back( { new unsigned char [64], 64 } );
        available_128.push_back( { new unsigned char [128], 128 } );
        available_256.push_back( { new unsigned char [256], 256 } );
    }
    for(unsigned int i = 0; i < sz1k ; i++) {
        available_1k.push_back( { new unsigned char [1 * 1024], 1 * 1024 } );
    }
    for(unsigned int i = 0; i < sz5k ; i++) {
        available_5k.push_back( { new unsigned char [5 * 1024], 5 * 1024 } );
    }
    for(unsigned int i = 0; i < sz10k ; i++) {
        available_10k.push_back( { new unsigned char [10 * 1024], 10 * 1024 } );
    }
    for(unsigned int i = 0; i < sz20k ; i++) {
        available_20k.push_back( { new unsigned char [20 * 1024], 20 * 1024 } );
    }
}

mem_chunk_t memPool::acquire(std::size_t sz) {

    if(MEMPOOL_DEBUG) {
        ERR_("mempool::acquire(%d)", sz);
    }

    if(sz == 0) return { nullptr, 0 };

    std::vector<mem_chunk_t>* mem_pool = pick_acq_set(sz);

    std::lock_guard<std::mutex> g(lock);
    stat_acq++;
    stat_acq_size += sz;

    if (mem_pool->empty()) {
        mem_chunk_t new_entry = { new unsigned char [sz], sz };
        stat_alloc++;
        stat_alloc_size += sz;

        return new_entry;
    } else {
        mem_chunk_t free_entry = mem_pool->back();
        mem_pool->pop_back();

        return free_entry;
    }
}


void memPool::release(mem_chunk_t to_ret){

    if(MEMPOOL_DEBUG) {
        ERR_("mempool::release(0x%x,%d)", to_ret.ptr, to_ret.capacity);
    }

    if (!to_ret.ptr) {
        return;
    }

    std::vector<mem_chunk_t>* mem_pool = pick_ret_set(to_ret.capacity);


    if(!mem_pool) {
        stat_out_free++;
        stat_out_free_size += to_ret.capacity;

        delete[] to_ret.ptr;
    } else {
        stat_ret++;
        stat_ret_size += to_ret.capacity;

        std::lock_guard<std::mutex> g(lock);
        mem_pool->push_back(to_ret);
    }
}

std::vector<mem_chunk_t>* memPool::pick_acq_set(ssize_t s) {
    if      (s > 20 * 1024) return &available_big;
    else if (s > 10 * 1024) return &available_20k;
    else if (s >  5 * 1024) return &available_10k;
    else if (s >  1 * 1024) return &available_5k;
    else if (s >       256) return &available_1k;
    else if (s >       128) return &available_256;
    else if (s >       64) return &available_128;
    else if (s >       32) return &available_64;
    else return &available_32;
}

std::vector<mem_chunk_t>* memPool::pick_ret_set(ssize_t s) {
    if      (s == 20 * 1024) return  available_20k.size() < sz20k ? &available_20k : nullptr;
    else if (s == 10 * 1024) return  available_10k.size() < sz10k ? &available_10k : nullptr;
    else if (s ==  5 * 1024) return  available_5k.size() < sz5k ? &available_5k : nullptr;
    else if (s ==  1 * 1024) return  available_1k.size() < sz1k ? &available_1k : nullptr;
    else if (s ==       256) return  available_256.size() < sz256 ? &available_256 : nullptr;
    else if (s ==       128) return  available_128.size() < sz256 ? &available_128 : nullptr;
    else if (s ==        64) return  available_64.size() < sz256 ? &available_64 : nullptr;
    else if (s ==        32) return  available_32.size() < 10 * sz256 ? &available_32 : nullptr;
    else return nullptr;
}


void* mempool_alloc(size_t s) {

    if(!buffer::use_pool)
        return malloc(s);

    mem_chunk_t mch = buffer::pool.acquire(s);
    std::lock_guard<std::mutex> l(mempool_ptr_map_lock);

    mempool_ptr_map[mch.ptr] = mch.capacity;

    stat_mempool_alloc++;
    stat_mempool_alloc_size += s;

    return mch.ptr;
}

void* mempool_realloc(void* optr, size_t nsz) {

    if(!buffer::use_pool)
        return realloc(optr,nsz);

    size_t ptr_size = 0;
    if(optr) {

        std::lock_guard<std::mutex> l(mempool_ptr_map_lock);

        auto i = mempool_ptr_map.find(optr);
        if (i != mempool_ptr_map.end()) {
            ptr_size = (*i).second;
        } else {
            stat_mempool_realloc_miss++;
        }

    }
    mem_chunk_t old_m = { static_cast<unsigned char*>(optr), ptr_size };

    // if realloc asks for actually already fitting size, return old one
    if(ptr_size >= nsz) {
        stat_mempool_realloc_fitting++;
        return optr;
    }

    mem_chunk_t new_m = buffer::pool.acquire(nsz);

    if(!new_m.ptr) {

        buffer::pool.release(old_m);
        std::lock_guard<std::mutex> l(mempool_ptr_map_lock);

        auto i = mempool_ptr_map.find(optr);
        if (i != mempool_ptr_map.end()) {
            mempool_ptr_map.erase(i);
        } else {
            stat_mempool_realloc_miss++;
        }

        return nullptr;
    } else {

        if(ptr_size)
            memcpy(new_m.ptr,optr, nsz <= ptr_size ? nsz : ptr_size);

        buffer::pool.release(old_m);

        {
            std::lock_guard<std::mutex> l(mempool_ptr_map_lock);
            mempool_ptr_map[new_m.ptr] = new_m.capacity;
        }

        stat_mempool_realloc++;
        stat_mempool_realloc += (new_m.capacity - old_m.capacity);

        return static_cast<void*>(new_m.ptr);
    }
}


void mempool_free(void* optr) {

    std::lock_guard<std::mutex> l(mempool_ptr_map_lock);

    size_t ptr_size = 0;
    auto i = mempool_ptr_map.find(optr);
    if (i != mempool_ptr_map.end()) {

        ptr_size = (*i).second;
        mempool_ptr_map.erase(i);
    } else {
        stat_mempool_free_miss++;
    }

    stat_mempool_free++;
    stat_mempool_free_size += ptr_size;

    buffer::pool.release( {static_cast<unsigned char*>(optr), ptr_size } );
}


void* mempool_alloc(size_t s, const char* src, int line) {
    return mempool_alloc(s);
}
void* mempool_realloc(void* optr, size_t s, const char* src, int line) {
    return mempool_realloc(optr, s);
}
void mempool_free(void* optr, const char* src, int line) {
    mempool_free(optr);
}