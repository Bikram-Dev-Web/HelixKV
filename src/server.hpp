#pragma once

#include "command_handler.hpp"

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
    void handleClient(socket_t client_fd);

    int port_;

    CommandHandler handler_;
};


