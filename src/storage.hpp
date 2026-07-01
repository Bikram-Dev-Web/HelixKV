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
    std::unordered_map<std::string, std::uint64_t> expirations_; // Key -> Absolute epoch seconds expiration
    Persistence persistence_;
    std::mutex mutex_;

    // Background AOF Compaction State
    std::atomic<bool> is_rewriting_{false};
    std::atomic<bool> rewrite_done_{false};
    std::vector<std::string> rewrite_buffer_;
    std::thread rewrite_thread_;

    // Auto-Compaction Trigger configurations
    size_t last_rewrite_size_{0};
    void checkAutoRewrite();
    void rewriteAOF_Lockless();
    const size_t AUTO_AOF_REWRITE_MIN_SIZE = 1024;      // 1KB threshold for test viability
    const size_t AUTO_AOF_REWRITE_PERCENTAGE = 100;    // Compacts if size doubles (100% growth)

    // Active eviction checker (returns true if key was evicted)
    bool checkAndEvict(const std::string& key);

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

    // Key TTL Operations
    int expire(const std::string& key, std::uint64_t seconds);
    int64_t ttl(const std::string& key);
    void cleanupExpiredKeys();
};

