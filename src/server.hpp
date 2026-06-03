#pragma once

#include "command_handler.hpp"

class Server
{
public:
    Server(int port);

    void start();

private:
    void handleClient(int client_fd);

    int port_;

    CommandHandler handler_;
};


