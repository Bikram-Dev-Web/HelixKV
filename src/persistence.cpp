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

void Persistence::saveRDB(const std::string& rdb_filename, const std::unordered_map<std::string, std::string>& data, const std::unordered_map<std::string, std::uint64_t>& expirations)
{
    std::string temp_filename = rdb_filename + ".tmp";
    std::ofstream file(temp_filename, std::ios::binary);

    if(!file.is_open())
    {
        std::cerr << "Failed to open temporary RDB file for saving: " << temp_filename << "\n";
        return;
    }

    // Write Magic (8 bytes)
    file.write("HELIXRDB", 8);
    // Write Version (4 bytes)
    file.write("0001", 4);

    // Write size of map (uint64_t)
    std::uint64_t size = data.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));

    for(const auto& [key, value] : data)
    {
        std::uint8_t has_expire = 0;
        std::uint64_t expire_time = 0;
        auto exp_it = expirations.find(key);
        if (exp_it != expirations.end()) {
            has_expire = 1;
            expire_time = exp_it->second;
        }

        // Write has_expire flag
        file.write(reinterpret_cast<const char*>(&has_expire), sizeof(has_expire));
        if (has_expire) {
            file.write(reinterpret_cast<const char*>(&expire_time), sizeof(expire_time));
        }

        // Write key length & key data
        std::uint32_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), key_len);

        // Write value length & value data
        std::uint32_t val_len = value.size();
        file.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
        file.write(value.data(), val_len);
    }

    file.close();

    // Overwrite real RDB file
    std::remove(rdb_filename.c_str());
    if(std::rename(temp_filename.c_str(), rdb_filename.c_str()) != 0)
    {
        std::perror("Failed to finalize RDB snapshot rename");
    }
}

bool Persistence::loadRDB(const std::string& rdb_filename, std::unordered_map<std::string, std::string>& data, std::unordered_map<std::string, std::uint64_t>& expirations)
{
    std::ifstream file(rdb_filename, std::ios::binary);
    if(!file.is_open())
    {
        return false;
    }

    char magic[8];
    char version[4];

    file.read(magic, 8);
    file.read(version, 4);

    if (file.gcount() < 4 || std::string(magic, 8) != "HELIXRDB" || std::string(version, 4) != "0001") {
        std::cerr << "Invalid or corrupted RDB file header." << std::endl;
        file.close();
        return false;
    }

    std::uint64_t key_count = 0;
    file.read(reinterpret_cast<char*>(&key_count), sizeof(key_count));
    if (file.gcount() < static_cast<std::streamsize>(sizeof(key_count))) {
        file.close();
        return false;
    }

    std::uint64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for(std::uint64_t i = 0; i < key_count; ++i)
    {
        std::uint8_t has_expire = 0;
        file.read(reinterpret_cast<char*>(&has_expire), sizeof(has_expire));

        std::uint64_t expire_time = 0;
        if (has_expire) {
            file.read(reinterpret_cast<char*>(&expire_time), sizeof(expire_time));
        }

        std::uint32_t key_len = 0;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        
        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        std::uint32_t val_len = 0;
        file.read(reinterpret_cast<char*>(&val_len), sizeof(val_len));
        
        std::string value(val_len, '\0');
        file.read(&value[0], val_len);

        if (file.fail()) {
            std::cerr << "Error reading key-value pair from RDB." << std::endl;
            file.close();
            return false;
        }

        if (has_expire && expire_time <= now_sec) {
            continue; // Expired, discard
        }

        data[key] = value;
        if (has_expire) {
            expirations[key] = expire_time;
        }
    }

    file.close();
    return true;
}