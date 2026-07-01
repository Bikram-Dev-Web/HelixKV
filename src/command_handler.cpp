#include "command_handler.hpp"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

std::string CommandHandler::makeSimpleString(const std::string& s) {
    return "+" + s + "\r\n";
}

std::string CommandHandler::makeError(const std::string& err) {
    return "-" + err + "\r\n";
}

std::string CommandHandler::makeInteger(int64_t val) {
    return ":" + std::to_string(val) + "\r\n";
}

std::string CommandHandler::makeBulkString(const std::string& s) {
    return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
}

std::string CommandHandler::makeNullBulkString() {
    return "$-1\r\n";
}

std::string CommandHandler::makeArray(const std::vector<std::string>& items) {
    std::string res = "*" + std::to_string(items.size()) + "\r\n";
    for (const auto& item : items) {
        res += makeBulkString(item);
    }
    return res;
}

std::string CommandHandler::handle(const std::string& command) {
    std::stringstream ss(command);
    std::string segment;
    std::vector<std::string> parts;

    while (ss >> segment) {
        parts.push_back(segment);
    }

    if (parts.empty()) return "ERR empty command\n";
    return handleCommand(parts, false);
}

std::string CommandHandler::handleCommand(const std::vector<std::string>& parts, bool is_resp) {
    if (parts.empty()) {
        return is_resp ? makeError("ERR empty command") : "ERR empty command\n";
    }

    std::string cmd = parts[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) {
        return std::toupper(c);
    });

    if (cmd == "PING") {
        return is_resp ? makeSimpleString("PONG") : "PONG\n";
    }
    else if (cmd == "SET") {
        if (parts.size() < 3) {
            return is_resp ? makeError("ERR usage: SET key value") : "ERR usage: SET key value\n";
        }
        storage_.set(parts[1], parts[2]);
        return is_resp ? makeSimpleString("OK") : "OK\n";
    }
    else if (cmd == "GET") {
        if (parts.size() < 2) {
            return is_resp ? makeError("ERR usage: GET key") : "ERR usage: GET key\n";
        }
        std::string val = storage_.get(parts[1]);
        if (val == "NULL") {
            return is_resp ? makeNullBulkString() : "NULL\n";
        }
        return is_resp ? makeBulkString(val) : val + "\n";
    }
    else if (cmd == "DEL") {
        if (parts.size() < 2) {
            return is_resp ? makeError("ERR usage: DEL key") : "ERR usage: DEL key\n";
        }
        bool existed = storage_.exist(parts[1]);
        if (existed) {
            storage_.del(parts[1]);
            return is_resp ? makeInteger(1) : "DELETED\n";
        }
        return is_resp ? makeInteger(0) : "\n";
    }
    else if (cmd == "EXISTS") {
        if (parts.size() < 2) {
            return is_resp ? makeError("ERR usage: EXISTS key") : "ERR usage: EXISTS key\n";
        }
        bool exists = storage_.exist(parts[1]);
        return is_resp ? makeInteger(exists ? 1 : 0) : (exists ? "1\n" : "0\n");
    }
    else if (cmd == "KEYS") {
        auto keys_list = storage_.keys();
        if (is_resp) {
            return makeArray(keys_list);
        }
        std::string res;
        for (size_t i = 0; i < keys_list.size(); ++i) {
            res += keys_list[i];
            if (i + 1 < keys_list.size()) {
                res += " ";
            }
        }
        return res + "\n";
    }
    else if (cmd == "SIZE") {
        size_t sz = storage_.size();
        return is_resp ? makeInteger(sz) : std::to_string(sz) + "\n";
    }
    else if (cmd == "CLEAR") {
        storage_.clear();
        return is_resp ? makeSimpleString("OK") : "OK\n";
    }
    else if (cmd == "EXPIRE") {
        if (parts.size() < 3) {
            return is_resp ? makeError("ERR usage: EXPIRE key seconds") : "ERR usage: EXPIRE key seconds\n";
        }
        try {
            std::uint64_t seconds = std::stoull(parts[2]);
            int res = storage_.expire(parts[1], seconds);
            return is_resp ? makeInteger(res) : std::to_string(res) + "\n";
        } catch (...) {
            return is_resp ? makeError("ERR value is not an integer or out of range") : "ERR value is not an integer or out of range\n";
        }
    }
    else if (cmd == "TTL") {
        if (parts.size() < 2) {
            return is_resp ? makeError("ERR usage: TTL key") : "ERR usage: TTL key\n";
        }
        int64_t res = storage_.ttl(parts[1]);
        return is_resp ? makeInteger(res) : std::to_string(res) + "\n";
    }
    else if (cmd == "BGREWRITEAOF") {
        if (storage_.isRewriting()) {
            return is_resp ? makeError("ERR Background rewrite already in progress") : "ERR Background rewrite already in progress\n";
        }
        storage_.rewriteAOF();
        return is_resp ? makeSimpleString("Background rewrite of AOF started") : "Background rewrite of AOF started\n";
    }
    else if (cmd == "INFO") {
        std::string info = "# Server\n";
        info += "helixkv_version:0.1.0\n";
        info += "port:6379\n";
        info += "# Keyspace\n";
        info += "db0:keys=" + std::to_string(storage_.size()) + "\n";
        return is_resp ? makeBulkString(info) : info + "\n";
    }

    return is_resp ? makeError("ERR UNKNOWN COMMAND") : "ERR UNKNOWN COMMAND\n";
}

Storage& CommandHandler::getStorage() {
    return storage_;
}