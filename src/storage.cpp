#include "storage.hpp"
#include "persistence.hpp"

using namespace std;


Storage::Storage()
{
    data_ = persistence_.load();
}

void Storage::set(const string& key , const string& value){
    data_[key]=value;
    persistence_.save(data_);
}

string Storage::get(const string& key){
    auto it = data_.find(key);

    if(it==data_.end()){
        return "NULL";
    }

    return it->second;
}

void Storage::del(const string& key){
    auto it =  data_.find(key);

    if(it==data_.end()){
        return ;
    }

    data_.erase(key);
    persistence_.save(data_);
}

bool Storage::exist(const string& key){
    auto it =  data_.find(key);

    if(it==data_.end()){return false;}
    return true;
}