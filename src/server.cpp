#include "server.hpp"

#include <iostream>
#include <algorithm>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define CLOSE_SOCKET(s) closesocket(s)
  #define IS_VALIDSOCKET(s) ((s) != INVALID_SOCKET)
  #ifdef _MSC_VER
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
  #endif
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define CLOSE_SOCKET(s) ::close(s)
  #define IS_VALIDSOCKET(s) ((s) >= 0)
#endif

Server::Server(int port)
    : port_(port)
{
}

void Server::start()
{
#ifdef _WIN32
    WSADATA wsaData;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_err != 0) {
        std::cerr << "WSAStartup failed with error: " << wsa_err << std::endl;
        return;
    }
#endif

    socket_t server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if(!IS_VALIDSOCKET(server_fd))
    {
        perror("socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    // Allow port reuse immediately after shutdown
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if(bind(server_fd,
            (sockaddr*)&address,
            sizeof(address)) != 0)
    {
        perror("bind");
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    if(listen(server_fd,
              SOMAXCONN) != 0)
    {
        perror("listen");
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    std::cout
        << "Server listening on port "
        << port_
        << " (Single-Threaded Event Loop)"
        << std::endl;

    while(true)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        // Add the listener socket to read_fds
        FD_SET(server_fd, &read_fds);
        socket_t max_fd = server_fd;

        // Add all active client sockets to read_fds
        for(socket_t client_fd : clients_)
        {
            FD_SET(client_fd, &read_fds);
            if(client_fd > max_fd)
            {
                max_fd = client_fd;
            }
        }

        // Block until any socket has pending read operations or timeout expires (to check AOF rewrite status)
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms

        int activity = select((int)(max_fd + 1), &read_fds, nullptr, nullptr, &timeout);

        if(activity < 0)
        {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            perror("select");
            break;
        }

        // Handle incoming connection request on the listening socket
        if(FD_ISSET(server_fd, &read_fds))
        {
            socket_t client_fd =
                accept(server_fd,
                       nullptr,
                       nullptr);

            if(!IS_VALIDSOCKET(client_fd))
            {
                perror("accept");
            }
            else
            {
                std::cout << "Client connected\n";
                clients_.push_back(client_fd);
            }
        }

        // Process reading data from connected client sockets
        // We iterate on a copy of clients_ in case a disconnect modifies it
        std::vector<socket_t> active_clients = clients_;
        for(socket_t client_fd : active_clients)
        {
            if(FD_ISSET(client_fd, &read_fds))
            {
                handleClientData(client_fd);
            }
        }

        // Check if background AOF compaction thread has finished
        if(handler_.getStorage().isRewriteFinished())
        {
            handler_.getStorage().finalizeAOF();
        }
    }

    // Cleanup sockets on server exit
    for(socket_t client_fd : clients_)
    {
        CLOSE_SOCKET(client_fd);
    }
    clients_.clear();

    CLOSE_SOCKET(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void Server::handleClientData(socket_t client_fd)
{
    char buffer[1024] = {0};

    ssize_t bytes_received =
        recv(client_fd,
             buffer,
             sizeof(buffer) - 1,
             0);

    if(bytes_received == 0)
    {
        std::cout << "Client disconnected\n";
        CLOSE_SOCKET(client_fd);
        clients_.erase(std::remove(clients_.begin(), clients_.end(), client_fd), clients_.end());
        return;
    }

    if(bytes_received < 0)
    {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            std::cout << "Client disconnected abruptly (error: " << err << ")\n";
            CLOSE_SOCKET(client_fd);
            clients_.erase(std::remove(clients_.begin(), clients_.end(), client_fd), clients_.end());
        }
#else
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("recv");
            CLOSE_SOCKET(client_fd);
            clients_.erase(std::remove(clients_.begin(), clients_.end(), client_fd), clients_.end());
        }
#endif
        return;
    }

    std::string command(
        buffer,
        bytes_received
    );

    std::string response =
        handler_.handle(command);

    send(
        client_fd,
        response.c_str(),
        (int)response.size(),
        0
    );
}