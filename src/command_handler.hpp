#pragma once
#include<string>
#include "storage.hpp"

class CommandHandler
{
private:
    Storage storage_;
public:
    std::string handle(const std::string& command);
    Storage& getStorage();
};


