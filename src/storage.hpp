#pragma once

#include <string>
#include <unordered_map>
using namespace std; 

class Storage
{
private:
    unordered_map<string,string> data_;
public:
    void set(const string& key , const string& value);
    string get(const string& key);
};

