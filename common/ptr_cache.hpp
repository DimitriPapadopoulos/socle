/*
    Smithproxy- transparent proxy with SSL inspection capabilities.
    Copyright (c) 2014, Ales Stibal <astib@mag0.net>, All rights reserved.

    Smithproxy is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Smithproxy is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Smithproxy.  If not, see <http://www.gnu.org/licenses/>.
    
*/

#ifndef PTR_CACHE_HPP
 #define PTR_CACHE_HPP

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

template <class K, class T>
class ptr_cache {
public:
    ptr_cache(): auto_delete_(true) {}
    ptr_cache(bool auto_delete): auto_delete_(auto_delete) {}
    virtual ~ptr_cache() { clear(); if(default_value_ != nullptr && auto_delete_) { delete default_value_; }; }


    void invalidate() {
        std::lock_guard<std::recursive_mutex> l(lock_);
        
        for(auto it = cache().begin(); it < cache().end() ; ++ it) {
            T*& ptr = it->second;
            if(auto_delete()) {
                delete ptr;
            }
            ptr = default_value();
        }
    }

    void clear() {
        std::lock_guard<std::recursive_mutex> l(lock_);
        cache().clear();
    }

    std::unordered_map<K,T*>& cache() { return cache_; }
    void lock() { lock_.lock(); };
    void unlock() { lock_.unlock(); };

    bool auto_delete() const { return auto_delete_; }
    void auto_delete(bool b) { auto_delete_ = b; }

    T*   default_value() const { return default_value_; }
    void default_value(T* d) const { if(default_value_ != nullptr && auto_delete_) { delete default_value_; }; default_value_ = d; }

    T* get(K& k) {
        auto it = cache().find(k);
        if(it == cache().end()) {
            return default_value();
        }
        return it->second;
    }
    
    // set the key->value. Return true if other value had been replaced.
    bool set(K& k, T* v) {
        bool ret = false;
        
        auto it = cache().find(k);
        if(it != cache().end()) {
            T*& ptr = it->second;
            if(ptr != nullptr) {
                ret = true;
                if(auto_delete()) {
                    delete ptr;
                }
            }
            ptr = v;
        } else {
            cache[k] = v;
        }

        return ret;
    }

private:
    bool auto_delete_ = true;
    T* default_value_ = nullptr;
    std::unordered_map<K,T*> cache_;
    std::recursive_mutex lock_;
};

#endif