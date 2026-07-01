#include "storage.hpp"
#include "persistence.hpp"
#include <fstream>
#include <iostream>

using namespace std;

Storage::Storage()
{
    data_ = persistence_.load();
    last_rewrite_size_ = persistence_.getFileSize();
}

Storage::~Storage()
{
    if (rewrite_thread_.joinable()) {
        rewrite_thread_.join();
    }
}

void Storage::set(const string& key , const string& value){
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key]=value;
    persistence_.append("SET", key, value);
    if (is_rewriting_) {
        rewrite_buffer_.push_back("SET " + key + " " + value);
    }
    checkAutoRewrite();
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
    if (is_rewriting_) {
        rewrite_buffer_.push_back("DEL " + key);
    }
    checkAutoRewrite();
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

    rewrite_thread_ = std::thread([this, snapshot]() {
        persistence_.rewrite(snapshot);
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