#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "persistence.hpp"
#include <mutex>

class Storage
{
private:
    std::unordered_map<std::string, std::string> data_;
    Persistence persistence_;
    std::mutex mutex_;
public:
    Storage();
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    void del(const std::string& key);
    bool exist(const std::string& key);
    std::vector<std::string> keys();
    size_t size();
    void clear();
    void rewriteAOF();
};

