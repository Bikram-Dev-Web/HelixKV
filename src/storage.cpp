#include "storage.hpp"

using namespace std;

void Storage::set(const string& key , const string& value){
    data_[key]=value;
}

string Storage::get(const string& key){
    auto it = data_.find(key);

    if(it==data_.end()){
        return "NULL";
    }

    return it->second;
}