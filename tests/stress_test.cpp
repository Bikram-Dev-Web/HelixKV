#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <random>
#include <cassert>
#include <atomic>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET socket_t;
  #define CLOSE_SOCKET(s) closesocket(s)
  #define IS_VALIDSOCKET(s) ((s) != INVALID_SOCKET)
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int socket_t;
  #define CLOSE_SOCKET(s) ::close(s)
  #define IS_VALIDSOCKET(s) ((s) >= 0)
#endif

std::atomic<int> active_connections{0};
std::atomic<long long> total_operations{0};
std::atomic<int> error_count{0};

void run_client_stress(const std::string& host, int port, int ops_count, int thread_id) {
    // Platform-specific socket startup
#ifdef _WIN32
    // Winsock is initialized once in main
#endif

    socket_t client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALIDSOCKET(client_fd)) {
        error_count++;
        return;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
#ifdef _WIN32
    serv_addr.sin_addr.s_addr = inet_addr(host.c_str());
#else
    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        error_count++;
        CLOSE_SOCKET(client_fd);
        return;
    }
#endif

    if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error_count++;
        CLOSE_SOCKET(client_fd);
        return;
    }

    active_connections++;

    // Random generator for operations
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist_cmd(0, 2); // SET, GET, DEL
    std::uniform_int_distribution<int> dist_key(1, 1000);

    char read_buf[2048];

    for (int i = 0; i < ops_count; ++i) {
        int cmd_type = dist_cmd(rng);
        int key_num = dist_key(rng);
        std::string cmd;

        if (cmd_type == 0) {
            cmd = "SET stress_key_" + std::to_string(thread_id) + "_" + std::to_string(key_num) + " value_" + std::to_string(i) + "\n";
        } else if (cmd_type == 1) {
            cmd = "GET stress_key_" + std::to_string(thread_id) + "_" + std::to_string(key_num) + "\n";
        } else {
            cmd = "DEL stress_key_" + std::to_string(thread_id) + "_" + std::to_string(key_num) + "\n";
        }

        // Send command
        int sent = send(client_fd, cmd.c_str(), (int)cmd.size(), 0);
        if (sent <= 0) {
            error_count++;
            break;
        }

        // Receive response
        int bytes_received = recv(client_fd, read_buf, sizeof(read_buf) - 1, 0);
        if (bytes_received <= 0) {
            error_count++;
            break;
        }
        
        total_operations++;
    }

    CLOSE_SOCKET(client_fd);
    active_connections--;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 6379;
    int num_threads = 100;
    int ops_per_thread = 1000;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);
    if (argc >= 4) num_threads = std::stoi(argv[3]);
    if (argc >= 5) ops_per_thread = std::stoi(argv[4]);

    std::cout << "Starting stress test on " << host << ":" << port << std::endl;
    std::cout << "Threads: " << num_threads << ", Operations per thread: " << ops_per_thread << std::endl;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(run_client_stress, host, port, ops_per_thread, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "\n=== Stress Test Results ===" << std::endl;
    std::cout << "Total Operations Sent: " << total_operations.load() << std::endl;
    std::cout << "Failed Operations/Errors: " << error_count.load() << std::endl;
    std::cout << "Elapsed Time: " << duration.count() << " seconds" << std::endl;
    if (duration.count() > 0) {
        std::cout << "Throughput: " << (total_operations.load() / duration.count()) << " ops/sec" << std::endl;
    }

    if (error_count.load() > 0) {
        std::cout << "STATUS: FAILED (Errors detected)" << std::endl;
        return 1;
    } else {
        std::cout << "STATUS: SUCCESS" << std::endl;
        return 0;
    }
}
