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
    persistence_.append("SET", key, value);
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
    persistence_.append("DEL", key);
}

bool Storage::exist(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it =  data_.find(key);

    if(it==data_.end()){return false;}
    return true;
}

std::vector<std::string> Storage::keys() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> all_keys;
    for (const auto& [key, _] : data_) {
        all_keys.push_back(key);
    }
    return all_keys;
}

size_t Storage::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

void Storage::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    persistence_.append("CLEAR", "");
}

void Storage::rewriteAOF() {
    std::lock_guard<std::mutex> lock(mutex_);
    persistence_.rewrite(data_);
}