#include "server.hpp"
#include<iostream>

Server::Server(int port):port_(port){}

void Server::start(){
    std::cout<<"server starting on port"
             <<port_
             <<std::endl;   
}