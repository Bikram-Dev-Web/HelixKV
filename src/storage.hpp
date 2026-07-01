#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include "persistence.hpp"

class Storage
{
private:
    std::unordered_map<std::string, std::string> data_;
    Persistence persistence_;
    std::mutex mutex_;

    // Background AOF Compaction State
    std::atomic<bool> is_rewriting_{false};
    std::atomic<bool> rewrite_done_{false};
    std::vector<std::string> rewrite_buffer_;
    std::thread rewrite_thread_;

public:
    Storage();
    ~Storage();

    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    void del(const std::string& key);
    bool exist(const std::string& key);
    std::vector<std::string> keys();
    size_t size();
    void clear();
    void rewriteAOF();

    // Async finalization and state checking
    bool isRewriteFinished();
    void finalizeAOF();
    bool isRewriting() const;
};

