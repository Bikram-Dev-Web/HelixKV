#pragma once

#include "command_handler.hpp"
#include <vector>

#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET socket_t;
#else
  typedef int socket_t;
#endif

class Server
{
public:
    Server(int port);

    void start();

private:
    void handleClientData(socket_t client_fd);

    int port_;

    CommandHandler handler_;

    std::vector<socket_t> clients_;
};


