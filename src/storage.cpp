#include "storage.hpp"
#include "persistence.hpp"

using namespace std;


Storage::Storage()
{
    data_ = persistence_.load();
}

void Storage::set(const string& key , const string& value){
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key]=value;
    persistence_.save(data_);
}

string Storage::get(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);

    if(it==data_.end()){
        return "NULL";
    }

    return it->second;
}

void Storage::del(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it =  data_.find(key);

    if(it==data_.end()){
        return ;
    }

    data_.erase(key);
    persistence_.save(data_);
}

bool Storage::exist(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it =  data_.find(key);

    if(it==data_.end()){return false;}
    return true;
}