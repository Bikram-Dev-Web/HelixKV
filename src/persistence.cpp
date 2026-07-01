#include "persistence.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>

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
}

std::string Persistence::getFilename() const
{
    return filename_;
}

void Persistence::rewrite(const std::unordered_map<std::string, std::string>& data)
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
    }
    file.close();
}

std::unordered_map<std::string, std::string> Persistence::load()
{
    std::unordered_map<std::string, std::string> data;
    std::ifstream file(filename_);

    if(!file.is_open())
    {
        return data;
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
                }
            }
            else if(op == "DEL")
            {
                if(ss >> key)
                {
                    data.erase(key);
                }
            }
            else if(op == "CLEAR")
            {
                data.clear();
            }
        }
    }

    return data;
}