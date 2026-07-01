#pragma once

#include <string>
#include <unordered_map>

class Persistence
{
private:
    std::string filename_;
public:
    explicit Persistence(const std::string& filename = "helixkv.aof");

    std::string getFilename() const;
    size_t getFileSize() const;

    void append(const std::string& op, const std::string& key, const std::string& value = "");

    void rewrite(const std::unordered_map<std::string, std::string>& data);

    std::unordered_map<std::string , std::string> load();


};
