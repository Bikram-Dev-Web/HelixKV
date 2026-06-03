#pragma once

#include <string>
#include <unordered_map>
#include "persistence.hpp"
using namespace std; 

class Storage
{
private:
    unordered_map<string,string> data_;
    Persistence persistence_;
public:
    Storage();
    void set(const string& key , const string& value);
    string get(const string& key);
    void del(const string& key);
    bool exist(const string& key);
};

