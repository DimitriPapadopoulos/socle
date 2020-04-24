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

#include <sobject.hpp>

namespace socle {


#ifdef SOCLE_MEM_PROFILE
bool sobject_info::enable_bt_ = true;
#else
bool sobject_info::enable_bt_ = false;
#endif

std::string sobject_info::to_string(int verbosity) const {
    std::stringstream r;
    
    if(verbosity > INF) {
        r << "    " << name() << ": age: " << age() << "s";
        
        if(verbosity >= DEB ) {
            std::string ex = extra_string();
            if(! ex.empty() )
                r << " extra info: " << ex;
        }
    }
    
    return r.str();
}

sobject::sobject() {
    std::lock_guard<std::recursive_mutex> l_(db().getlock());

    static const uint64_t id_start = 0xCABA1ACABA1A;
    static const uint64_t id_key =   0x3453ABC3450F;
    static uint64_t id_current = id_start;

    oid_ = id_key ^ id_current++;

    oid_db().set(oid_, this);
    db().set(this,new sobject_info());
    mtr_created().update(1);
}


sobject::~sobject() {
    std::lock_guard<std::recursive_mutex> l_(db().getlock());

    oid_db().erase(oid());
    db().erase(this);
    mtr_deleted().update(1);
}


std::string sobjectDB::str_list(const char* class_criteria, const char* delimiter, int verbosity, const char* content_criteria) {
    
    std::stringstream ret;
    std::string criteria;

    auto& log = sobjectDB::get().log;

    std::lock_guard<std::recursive_mutex> l_(db().getlock());
    
    if(class_criteria)
        criteria = class_criteria;
    
    for(auto it: db().cache()) {
        sobject*       ptr = it.first;

        if(!ptr) continue;
        
        bool matched = true;
        
        //having criteria specified, select if it's name match, or pointer match
        if(criteria.length()) {

            if(criteria.compare(0,2,"0x") == 0) {
                std::string str_ptr = string_format("0x%lx",ptr);
                
                _deb("comparing pointer: %s and %s",str_ptr.c_str(), criteria.c_str());
                matched = (str_ptr == criteria);
            }
            else if(criteria.compare(0,3,"oid") == 0) {

                auto find_oid = criteria.substr(3);
                matched = (std::to_string(ptr->oid()) == find_oid );
            }
            else {
                _deb("comparing classname: %s and %s",ptr->class_name().c_str(), criteria.c_str());
                matched = (ptr->class_name() == criteria || criteria == "*");
            }
        }
        
        
        if(matched) {
            sobject_info*  si = it.second;
            std::string obj_string = ptr->to_string(verbosity);
            
            if(content_criteria) {
                if(obj_string.find(content_criteria) == std::string::npos) { continue; }
            }

            ret << string_format("OID: %llx ptr: 0x%lx | ", ptr->oid(), ptr) + obj_string;

            if(verbosity >= DEB) {
                ret << "\n";
                if(si != nullptr) 
                    ret << si->to_string(verbosity);
            }
            
            (delimiter == nullptr) ? ret << "\n" : ret << delimiter;
            
        }
    }
    
    return ret.str();
}


std::string sobjectDB::str_stats(const char* criteria) {

    auto& log = sobjectDB::get().log;

    std::stringstream ret;
    std::lock_guard<std::recursive_mutex> l_(db().getlock());

    unsigned long object_counter = 0;
    
    unsigned int youngest_age = -1;
    unsigned int oldest_age = -1;
    float sum_age = 0.0;
    
    for(auto it: db().cache()) {
        sobject*       ptr = it.first;

        if(! ptr) {
            continue;
        }
        _deb("comparing classname: %s and %s",ptr->c_class_name(), criteria);
        if( criteria == nullptr || ptr->class_name() == criteria ) {
            sobject_info*  si = it.second;
            object_counter++;
            
            if(si != nullptr) {
                unsigned int a = si->age();
                sum_age += static_cast<float>(a);
                
                if(a > oldest_age) oldest_age = a;
                if(a < youngest_age || youngest_age < 0) youngest_age = a;
            }
            
        }
    }

    float avg_age = 0;
    if (object_counter > 0) 
        avg_age = sum_age/static_cast<float>(object_counter);
    
    ret << "Performance: " << socle::sobject::mtr_created().get() << " new objects per second, "
                           << socle::sobject::mtr_deleted().get() << " deleted objects per second.\n";

    ret << "Database contains: "<< object_counter << " matching entries (" << ( criteria ? criteria : "*" ) << "), oldest " << static_cast<int>(oldest_age) << "s, ";
    ret << "youngest age "<< youngest_age << "s, average age is "<< avg_age << "s.";
    ret << "\n";
    ret << "Full DB size: " << db().cache().size() << " full OID DB size: " << oid_db().cache().size();
    return ret.str();
}

// asks object to terminate
int sobjectDB::ask_destroy(void* ptr) {
    
    int ret = -1;

    std::lock_guard<std::recursive_mutex> l_(sobjectDB::db().getlock());
    
    auto it = db().cache().find((sobject*)ptr);
    if(it != db().cache().end()) {
        ret = 0;
        if(it->first->ask_destroy()) {
            ret = 1;
        }
    }

    return ret;
}

long unsigned int meter::update(unsigned long val) {
    
    auto now = std::chrono::system_clock::now();

    total_ += val;

    if( now - last_update > std::chrono::seconds(interval_)) {

        int missed_scores = (now - last_update)/std::chrono::seconds(interval_);
        if(missed_scores > scoreboard_sz) missed_scores = scoreboard_sz;

        // threshold is reached => counter contains all bytes in previous second
        last_update = now;
        cnt_updates ++;

        push_score(prev_counter_);

        // push zero for missed values
        for(int i = 0; i < missed_scores ; i++) {
            push_score(0);
        }

        prev_counter_  = curr_counter_;
        
        curr_counter_ = val;
        
    } else {
        curr_counter_ += val;
    }
    
    return prev_counter_;
}

    unsigned long update(unsigned long val);

    unsigned long meter::get() const {

        auto now = std::chrono::system_clock::now();

        if( now > last_update + (2 + scoreboard_sz) * std::chrono::seconds(interval_)) {
            // not updated for a while
            return 0;
        }

//        if(now > last_update) {
//
            unsigned long divisor = (2+scoreboard_sz )*interval_;

            if(cnt_updates < 2 + scoreboard_sz) {
                divisor = cnt_updates*interval_;
            }

            if(! divisor) {
                divisor = 1;
            }

            // we are in the window if this update
            return ( curr_counter_ + prev_counter_ + sum_score() )
                     /
                   ( divisor );
//        }
//
//        return prev_counter_;
    }
}