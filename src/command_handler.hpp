#pragma once
#include <string>
#include <vector>
#include "storage.hpp"

class CommandHandler
{
private:
    Storage storage_;

    // RESP Formatting helpers
    std::string makeSimpleString(const std::string& s);
    std::string makeError(const std::string& err);
    std::string makeInteger(int64_t val);
    std::string makeBulkString(const std::string& s);
    std::string makeNullBulkString();
    std::string makeArray(const std::vector<std::string>& items);

public:
    std::string handle(const std::string& command);
    std::string handleCommand(const std::vector<std::string>& parts, bool is_resp);
    Storage& getStorage();
};


