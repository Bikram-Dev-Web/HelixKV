#include "persistence.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <chrono>

Persistence::Persistence(const std::string& filename)
    : filename_(filename)
{
}

void Persistence::append(const std::string& op, const std::string& key, const std::string& value)
{
    std::ofstream file(filename_, std::ios::app);

    if(!file.is_open())
    {
        std::cerr << "Failed to open AOF file for appending: " << filename_ << "\n";
        return;
    }

    if(op == "SET")
    {
        file << "SET " << key << " " << value << "\n";
    }
    else if(op == "DEL")
    {
        file << "DEL " << key << "\n";
    }
    else if(op == "CLEAR")
    {
        file << "CLEAR\n";
    }
    else if(op == "EXPIREAT")
    {
        file << "EXPIREAT " << key << " " << value << "\n";
    }
}

std::string Persistence::getFilename() const
{
    return filename_;
}

size_t Persistence::getFileSize() const
{
    std::ifstream file(filename_, std::ios::binary | std::ios::ate);
    return file.is_open() ? static_cast<size_t>(file.tellg()) : 0;
}

void Persistence::rewrite(const std::unordered_map<std::string, std::string>& data, const std::unordered_map<std::string, std::uint64_t>& expirations)
{
    std::string temp_filename = filename_ + ".tmp";
    std::ofstream file(temp_filename);

    if(!file.is_open())
    {
        std::cerr << "Failed to open temporary AOF file for rewriting: " << temp_filename << "\n";
        return;
    }

    for(const auto& [key, value] : data)
    {
        file << "SET " << key << " " << value << "\n";
        auto it = expirations.find(key);
        if (it != expirations.end()) {
            file << "EXPIREAT " << key << " " << it->second << "\n";
        }
    }
    file.close();
}

void Persistence::load(std::unordered_map<std::string, std::string>& data, std::unordered_map<std::string, std::uint64_t>& expirations)
{
    std::ifstream file(filename_);

    if(!file.is_open())
    {
        return;
    }

    std::string line;

    while(std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string op, key;

        if(ss >> op)
        {
            if(op == "SET")
            {
                if(ss >> key)
                {
                    std::string value;
                    ss >> std::ws;
                    std::getline(ss, value);
                    data[key] = value;
                    expirations.erase(key);
                }
            }
            else if(op == "DEL")
            {
                if(ss >> key)
                {
                    data.erase(key);
                    expirations.erase(key);
                }
            }
            else if(op == "CLEAR")
            {
                data.clear();
                expirations.clear();
            }
            else if(op == "EXPIREAT")
            {
                std::string timestamp_str;
                if(ss >> key >> timestamp_str)
                {
                    try {
                        expirations[key] = std::stoull(timestamp_str);
                    } catch (...) {
                        // ignore malformed lines
                    }
                }
            }
        }
    }
    file.close();

    // Evict keys that expired while the server was offline
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::vector<std::string> expired_keys;
    for (const auto& [key, expire_time] : expirations) {
        if (expire_time <= static_cast<std::uint64_t>(now_sec)) {
            expired_keys.push_back(key);
        }
    }

    for (const auto& key : expired_keys) {
        data.erase(key);
        expirations.erase(key);
    }
}