#pragma once

class Server
{
private:
    int port_;

public:
    Server(int port);
    void start();
};


