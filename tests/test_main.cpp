#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>
#include <chrono>
#include "storage.hpp"
#include "persistence.hpp"
#include "command_handler.hpp"

// Helper to clean up test files
void cleanup_files() {
    std::remove("test_helixkv.aof");
    std::remove("test_helixkv.aof.tmp");
}

void test_storage_basic() {
    std::cout << "Running test_storage_basic..." << std::endl;
    // Force specific file for testing
    cleanup_files();
    
    {
        Storage db;
        db.clear();
        assert(db.size() == 0);

        db.set("a", "1");
        assert(db.get("a") == "1");
        assert(db.exist("a") == true);
        assert(db.size() == 1);

        db.set("b", "2");
        assert(db.get("b") == "2");
        assert(db.size() == 2);

        db.del("a");
        assert(db.get("a") == "NULL");
        assert(db.exist("a") == false);
        assert(db.size() == 1);

        db.clear();
        assert(db.size() == 0);
        assert(db.get("b") == "NULL");
    }
    cleanup_files();
    std::cout << "test_storage_basic passed!" << std::endl;
}

void test_persistence_aof_replay() {
    std::cout << "Running test_persistence_aof_replay..." << std::endl;
    cleanup_files();

    // Create a mock AOF file
    {
        Persistence p("test_helixkv.aof");
        p.append("SET", "x", "100");
        p.append("SET", "y", "200");
        p.append("DEL", "x");
        p.append("SET", "z", "300");
    }

    // Load it via Storage and verify correct replay state
    {
        std::unordered_map<std::string, std::string> data;
        std::unordered_map<std::string, std::uint64_t> expirations;
        Persistence p("test_helixkv.aof");
        p.load(data, expirations);
        assert(data.size() == 2);
        assert(data["y"] == "200");
        assert(data["z"] == "300");
        assert(data.find("x") == data.end());
    }

    cleanup_files();
    std::cout << "test_persistence_aof_replay passed!" << std::endl;
}

void test_command_handler() {
    std::cout << "Running test_command_handler..." << std::endl;
    CommandHandler handler;
    handler.getStorage().clear();

    // Test PING
    assert(handler.handle("PING") == "PONG\n");
    assert(handler.handle("ping") == "PONG\n"); // Case-insensitivity

    // Test SET
    assert(handler.handle("set name alice") == "OK\n");
    assert(handler.handle("GET name") == "alice\n"); // Key names are case-sensitive, commands are not
    assert(handler.handle("Get name") == "alice\n");
    assert(handler.handle("GET NAME") == "NULL\n");   // Uppercase key does not exist

    // Test EXISTS
    assert(handler.handle("exists name") == "1\n");
    assert(handler.handle("exists age") == "0\n");

    // Test SIZE
    assert(handler.handle("size") == "1\n");

    // Test KEYS
    assert(handler.handle("keys") == "name\n");

    // Test DEL
    assert(handler.handle("del name") == "DELETED\n");
    assert(handler.handle("get name") == "NULL\n");
    assert(handler.handle("size") == "0\n");

    // Test malformed commands
    assert(handler.handle("SET key") == "ERR usage: SET key value\n");
    assert(handler.handle("GET") == "ERR usage: GET key\n");
    assert(handler.handle("EXISTS") == "ERR usage: EXISTS key\n");
    assert(handler.handle("UNKNOWN") == "ERR UNKNOWN COMMAND\n");

    std::cout << "test_command_handler passed!" << std::endl;
}

void test_asynchronous_compaction() {
    std::cout << "Running test_asynchronous_compaction..." << std::endl;
    // We will test the async write threading logic directly
    {
        Storage db;
        db.clear();
        db.set("k1", "v1");
        db.set("k2", "v2");

        // Trigger compaction
        db.rewriteAOF();
        assert(db.isRewriting() == true);

        // Immediately write during compaction (should be buffered)
        db.set("k3", "v3");
        db.set("k2", "v2_modified");

        // Wait for thread to finish
        int retries = 0;
        while (!db.isRewriteFinished() && retries < 20) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            retries++;
        }
        assert(db.isRewriteFinished() == true);

        // Finalize
        db.finalizeAOF();
        assert(db.isRewriting() == false);

        // Check if data is correct
        assert(db.get("k1") == "v1");
        assert(db.get("k2") == "v2_modified");
        assert(db.get("k3") == "v3");
    }
    std::cout << "test_asynchronous_compaction passed!" << std::endl;
}

void test_key_expiration_and_ttls() {
    std::cout << "Running test_key_expiration_and_ttls..." << std::endl;
    cleanup_files();
    {
        Storage db;
        db.clear();
        
        // Expiry of non-existent key
        assert(db.expire("nonexistent", 10) == 0);
        assert(db.ttl("nonexistent") == -2);

        // Expiry of active key
        db.set("temp", "hello");
        assert(db.ttl("temp") == -1); // No expire
        assert(db.expire("temp", 2) == 1);
        
        int64_t remaining = db.ttl("temp");
        assert(remaining > 0 && remaining <= 2);

        // Sleep to wait for expiration
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));

        // Active eviction check
        assert(db.get("temp") == "NULL");
        assert(db.ttl("temp") == -2);
        assert(db.exist("temp") == false);
    }
    
    // Test persistence AOF replay time durability
    {
        cleanup_files();
        {
            Persistence p("test_helixkv.aof");
            p.append("SET", "t1", "v1");
            
            std::uint64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            p.append("EXPIREAT", "t1", std::to_string(now_sec + 10)); // Expires in 10s
            
            p.append("SET", "t2", "v2");
            p.append("EXPIREAT", "t2", std::to_string(now_sec - 5));  // Already expired
        }
        
        // Load back and assert
        std::unordered_map<std::string, std::string> data;
        std::unordered_map<std::string, std::uint64_t> expirations;
        Persistence p("test_helixkv.aof");
        p.load(data, expirations);
        
        // t1 should survive, t2 should be discarded
        assert(data.count("t1") == 1);
        assert(data["t1"] == "v1");
        assert(expirations.count("t1") == 1);
        
        assert(data.count("t2") == 0);
        assert(expirations.count("t2") == 0);
    }
    
    cleanup_files();
    std::cout << "test_key_expiration_and_ttls passed!" << std::endl;
}

void test_resp_protocol_formatting() {
    std::cout << "Running test_resp_protocol_formatting..." << std::endl;
    CommandHandler handler;
    handler.getStorage().clear();

    // Test PING
    assert(handler.handleCommand({"PING"}, true) == "+PONG\r\n");

    // Test SET
    assert(handler.handleCommand({"SET", "mykey", "myval"}, true) == "+OK\r\n");

    // Test GET
    assert(handler.handleCommand({"GET", "mykey"}, true) == "$5\r\nmyval\r\n");
    assert(handler.handleCommand({"GET", "nonexistent"}, true) == "$-1\r\n");

    // Test EXISTS
    assert(handler.handleCommand({"EXISTS", "mykey"}, true) == ":1\r\n");
    assert(handler.handleCommand({"EXISTS", "nonexistent"}, true) == ":0\r\n");

    // Test SIZE
    assert(handler.handleCommand({"SIZE"}, true) == ":1\r\n");

    // Test KEYS
    assert(handler.handleCommand({"KEYS"}, true) == "*1\r\n$5\r\nmykey\r\n");

    // Test DEL
    assert(handler.handleCommand({"DEL", "mykey"}, true) == ":1\r\n");
    assert(handler.handleCommand({"DEL", "mykey"}, true) == ":0\r\n");

    std::cout << "test_resp_protocol_formatting passed!" << std::endl;
}

int main() {
    std::cout << "=== HelixKV Test Suite ===" << std::endl;
    
    test_storage_basic();
    test_persistence_aof_replay();
    test_command_handler();
    test_asynchronous_compaction();
    test_key_expiration_and_ttls();
    test_resp_protocol_formatting();

    std::cout << "\nALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
