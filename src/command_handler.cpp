#include "command_handler.hpp"
#include<sstream>
#include<vector>



std::string CommandHandler::handle(const std::string& command){

    std::stringstream ss(command);
    std:: string segment;
    std:: vector<std::string> parts;

    while(ss>>segment){
        parts.push_back(segment);
    }

    if(parts.empty()) return "ERR empty commnd\n";

    // route the commnd
    std::string cmd = parts[0];

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

    return "ERR UNKNOWN COMMAND\n";
}