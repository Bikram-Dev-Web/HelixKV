#include "command_handler.hpp"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

std::string CommandHandler::handle(const std::string& command){

    std::stringstream ss(command);
    std::string segment;
    std::vector<std::string> parts;

    while(ss>>segment){
        parts.push_back(segment);
    }

    if(parts.empty()) return "ERR empty command\n";

    // route the command (case-insensitive conversion for the command name)
    std::string cmd = parts[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) {
        return std::toupper(c);
    });

    if(cmd=="PING"){return "PONG\n";}

    else if(cmd=="SET"){
        if(parts.size()<3)return "ERR usage: SET key value\n";
        storage_.set(parts[1],parts[2]);
        return "OK\n";
    }
    else if(cmd=="GET"){
        if(parts.size()<2)return "ERR usage: GET key\n";
        return storage_.get(parts[1]) + "\n";
    }
    else if (cmd=="DEL"){
        if(parts.size()<2)return "ERR usage: DEL key\n";
        storage_.del(parts[1]);
        return "DELETED\n";
    }
    else if(cmd=="EXISTS"){
        if(parts.size()<2)return "ERR usage: EXISTS key\n";
        bool ans = storage_.exist(parts[1]);
        return std::to_string(ans) + "\n";
    }
    else if(cmd=="KEYS"){
        auto keys_list = storage_.keys();
        std::string res;
        for (size_t i = 0; i < keys_list.size(); ++i) {
            res += keys_list[i];
            if (i + 1 < keys_list.size()) {
                res += " ";
            }
        }
        return res + "\n";
    }
    else if(cmd=="SIZE"){
        return std::to_string(storage_.size()) + "\n";
    }
    else if(cmd=="CLEAR"){
        storage_.clear();
        return "OK\n";
    }
    else if(cmd=="INFO"){
        std::string info = "# Server\n";
        info += "helixkv_version:0.1.0\n";
        info += "port:6379\n";
        info += "# Keyspace\n";
        info += "db0:keys=" + std::to_string(storage_.size()) + "\n";
        return info;
    }

    return "ERR UNKNOWN COMMAND\n";
}