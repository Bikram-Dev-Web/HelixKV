#include "server.hpp"

#include <iostream>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

Server::Server(int port)
    : port_(port)
{
}

void Server::start()
{
    int server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if(server_fd < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if(bind(server_fd,
            (sockaddr*)&address,
            sizeof(address)) < 0)
    {
        perror("bind");
        return;
    }

    if(listen(server_fd,
              SOMAXCONN) < 0)
    {
        perror("listen");
        return;
    }

    std::cout
        << "Server listening on port "
        << port_
        << std::endl;

    while(true)
    {
        int client_fd =
            accept(server_fd,
                   nullptr,
                   nullptr);

        if(client_fd < 0)
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
}

void Server::handleClient(int client_fd)
{
    while(true)
    {
        char buffer[1024] = {0};

        ssize_t bytes_received =
            recv(client_fd,
                 buffer,
                 sizeof(buffer),
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

    close(client_fd);
}