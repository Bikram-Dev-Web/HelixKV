#include "server.hpp"

#include <iostream>
#include <thread>

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
        << std::endl;

    while(true)
    {
        socket_t client_fd =
            accept(server_fd,
                   nullptr,
                   nullptr);

        if(!IS_VALIDSOCKET(client_fd))
        {
            perror("accept");
            continue;
        }

        std::cout
            << "Client connected\n";

        std::thread(
            &Server::handleClient,
            this,
            client_fd
        ).detach();
    }

    CLOSE_SOCKET(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void Server::handleClient(socket_t client_fd)
{
    while(true)
    {
        char buffer[1024] = {0};

        ssize_t bytes_received =
            recv(client_fd,
                 buffer,
                 sizeof(buffer) - 1,
                 0);

        if(bytes_received == 0)
        {
            std::cout
                << "Client disconnected\n";
            break;
        }

        if(bytes_received < 0)
        {
            perror("recv");
            break;
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
            response.size(),
            0
        );
    }

    CLOSE_SOCKET(client_fd);
}