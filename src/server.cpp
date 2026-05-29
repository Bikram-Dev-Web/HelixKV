#include "server.hpp"
#include "command_handler.hpp"
#include "storage.hpp"

#include <iostream>
#include <cstdio>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

Server::Server(int port)
    : port_(port)
{
}

void Server::start() {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd < 0) {
        perror("socket failed");
        return;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    bind(server_fd,
         (sockaddr*)&address,
         sizeof(address));

    listen(server_fd, SOMAXCONN);

    std::cout << "Server listening on port "
              << port_
              << std::endl;
             
    std::cout << "Waiting for client...\n";
    int client_fd = accept(server_fd,nullptr,nullptr);
    if(client_fd < 0){
      perror("accept failed");
      return;
    }
    std::cout << "Client connected!\n"; 


    CommandHandler handler;
    Storage storage_;
    while(true){
      char buffer[1024] = {0};
      ssize_t bytes_received = recv(client_fd , buffer , sizeof(buffer),0);

      // error handling
      if(bytes_received==0){std::cout<<"client disconnected"<<std::endl;break;}
      if(bytes_received<0){perror("recv");break;}

      std::string command(buffer , bytes_received);
      std::string response = handler.handle(command);



      



      std::cout<<buffer<<bytes_received<<std::endl;


      send(client_fd , response.c_str(),response.size(),0);
      // close(client_fd);
      // close(server_fd);
    }
         
}