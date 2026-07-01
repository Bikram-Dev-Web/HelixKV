#include "storage.hpp"
#include "persistence.hpp"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace std;

Storage::Storage()
{
    persistence_.load(data_, expirations_);
    last_rewrite_size_ = persistence_.getFileSize();
}

Storage::~Storage()
{
    if (rewrite_thread_.joinable()) {
        rewrite_thread_.join();
    }
}

bool Storage::checkAndEvict(const std::string& key) {
    auto it = expirations_.find(key);
    if (it == expirations_.end()) {
        return false;
    }

    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (it->second <= current_time) {
        data_.erase(key);
        expirations_.erase(it);
        persistence_.append("DEL", key);
        if (is_rewriting_) {
            rewrite_buffer_.push_back("DEL " + key);
        }
        return true;
    }
    return false;
}

void Storage::set(const string& key , const string& value){
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key]=value;
    expirations_.erase(key); // Clear TTL on overwrite
    persistence_.append("SET", key, value);
    if (is_rewriting_) {
        rewrite_buffer_.push_back("SET " + key + " " + value);
    }
    checkAutoRewrite();
}

string Storage::get(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    if (checkAndEvict(key)) {
        return "NULL";
    }
    auto it = data_.find(key);

    if(it==data_.end()){
        return "NULL";
    }

    return it->second;
}

void Storage::del(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    expirations_.erase(key);
    auto it =  data_.find(key);

    if(it==data_.end()){
        return ;
    }

    data_.erase(key);
    persistence_.append("DEL", key);
    if (is_rewriting_) {
        rewrite_buffer_.push_back("DEL " + key);
    }
    checkAutoRewrite();
}

bool Storage::exist(const string& key){
    std::lock_guard<std::mutex> lock(mutex_);
    if (checkAndEvict(key)) {
        return false;
    }
    auto it =  data_.find(key);

    if(it==data_.end()){return false;}
    return true;
}

std::vector<std::string> Storage::keys() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::string> expired_keys;
    for (const auto& [key, expire_time] : expirations_) {
        if (expire_time <= current_time) {
            expired_keys.push_back(key);
        }
    }

    for (const auto& key : expired_keys) {
        data_.erase(key);
        expirations_.erase(key);
        persistence_.append("DEL", key);
        if (is_rewriting_) {
            rewrite_buffer_.push_back("DEL " + key);
        }
    }

    std::vector<std::string> all_keys;
    for (const auto& [key, _] : data_) {
        all_keys.push_back(key);
    }
    return all_keys;
}

size_t Storage::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::string> expired_keys;
    for (const auto& [key, expire_time] : expirations_) {
        if (expire_time <= current_time) {
            expired_keys.push_back(key);
        }
    }

    for (const auto& key : expired_keys) {
        data_.erase(key);
        expirations_.erase(key);
        persistence_.append("DEL", key);
        if (is_rewriting_) {
            rewrite_buffer_.push_back("DEL " + key);
        }
    }
    return data_.size();
}

void Storage::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    expirations_.clear();
    persistence_.append("CLEAR", "");
    if (is_rewriting_) {
        rewrite_buffer_.push_back("CLEAR");
    }
    checkAutoRewrite();
}

void Storage::rewriteAOF() {
    std::lock_guard<std::mutex> lock(mutex_);
    rewriteAOF_Lockless();
}

void Storage::rewriteAOF_Lockless() {
    if (is_rewriting_) {
        return; // Compaction already running
    }

    is_rewriting_ = true;
    rewrite_done_ = false;
    rewrite_buffer_.clear();

    if (rewrite_thread_.joinable()) {
        rewrite_thread_.join();
    }

    std::unordered_map<std::string, std::string> snapshot = data_;
    std::unordered_map<std::string, std::uint64_t> snapshot_exp = expirations_;

    rewrite_thread_ = std::thread([this, snapshot, snapshot_exp]() {
        persistence_.rewrite(snapshot, snapshot_exp);
        rewrite_done_ = true;
    });
}

bool Storage::isRewriteFinished()
{
    return rewrite_done_;
}

bool Storage::isRewriting() const
{
    return is_rewriting_;
}

void Storage::finalizeAOF()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (rewrite_thread_.joinable()) {
        rewrite_thread_.join();
    }

    std::string filename = persistence_.getFilename();
    std::string temp_filename = filename + ".tmp";

    // Append buffered edits to the compacted file
    std::ofstream file(temp_filename, std::ios::app);
    if (file.is_open()) {
        for (const auto& cmd : rewrite_buffer_) {
            file << cmd << "\n";
        }
        file.close();
    } else {
        std::cerr << "Failed to open temporary file for finalization: " << temp_filename << "\n";
    }

    // Atomically swap the new file to active AOF
    std::remove(filename.c_str());
    if (std::rename(temp_filename.c_str(), filename.c_str()) != 0) {
        std::perror("Failed to rename temporary AOF file during finalization");
    }

    rewrite_buffer_.clear();
    is_rewriting_ = false;
    rewrite_done_ = false;

    // Update baseline compaction file size
    last_rewrite_size_ = persistence_.getFileSize();

    std::cout << "Background AOF rewrite finalized successfully." << std::endl;
}

void Storage::checkAutoRewrite() {
    if (is_rewriting_) {
        return;
    }
    size_t current_size = persistence_.getFileSize();
    if (current_size >= AUTO_AOF_REWRITE_MIN_SIZE) {
        if (last_rewrite_size_ == 0 || current_size >= last_rewrite_size_ * (1 + AUTO_AOF_REWRITE_PERCENTAGE / 100.0)) {
            std::cout << "Auto-compaction triggered. Current AOF size: " << current_size 
                      << " bytes, Last rewrite size: " << last_rewrite_size_ << " bytes." << std::endl;
            rewriteAOF_Lockless();
        }
    }
}

int Storage::expire(const std::string& key, std::uint64_t seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (checkAndEvict(key) || data_.find(key) == data_.end()) {
        return 0; // Key doesn't exist
    }

    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::uint64_t expire_time = current_time + seconds;
    expirations_[key] = expire_time;
    persistence_.append("EXPIREAT", key, std::to_string(expire_time));
    
    if (is_rewriting_) {
        rewrite_buffer_.push_back("EXPIREAT " + key + " " + std::to_string(expire_time));
    }
    return 1;
}

int64_t Storage::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (checkAndEvict(key) || data_.find(key) == data_.end()) {
        return -2; // Key doesn't exist
    }

    auto it = expirations_.find(key);
    if (it == expirations_.end()) {
        return -1; // Key exists but has no associated TTL
    }

    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (it->second <= current_time) {
        return -2; // Expired
    }
    return static_cast<int64_t>(it->second - current_time);
}

void Storage::cleanupExpiredKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::string> expired_keys;
    for (const auto& [key, expire_time] : expirations_) {
        if (expire_time <= current_time) {
            expired_keys.push_back(key);
        }
    }

    for (const auto& key : expired_keys) {
        data_.erase(key);
        expirations_.erase(key);
        persistence_.append("DEL", key);
        if (is_rewriting_) {
            rewrite_buffer_.push_back("DEL " + key);
        }
        std::cout << "Passively evicted expired key: " << key << std::endl;
    }
}