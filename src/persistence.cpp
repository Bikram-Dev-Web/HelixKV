#include "persistence.hpp"

#include<fstream>
#include<iostream>

Persistence::Persistence(const std::string& filename)
    :filename_(filename){

    }

void Persistence::save(const std::unordered_map<std::string , std::string>& data)    {
    std::ofstream file(filename_);

    if(!file.is_open()){
        std::cerr<<"Failed to open file for writing\n";
        return;
    }
    for(const auto& [key , value]:data){
        file<<key
            <<"="
            <<value
            <<"\n";
    }
}

std::unordered_map<std::string , std::string> Persistence::load(){
    std::unordered_map<std::string,std::string> data;

    std::ifstream file(filename_);

    if(!file.is_open())
    {
        return data;
    }

    std::string line;

    while(std::getline(file, line))
    {
        std::size_t pos = line.find('=');

        if(pos == std::string::npos)
        {
            continue;
        }

        std::string key =
            line.substr(0, pos);

        std::string value =
            line.substr(pos + 1);

        data[key] = value;
    }

    return data;

}