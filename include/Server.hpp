#pragma once
#include <queue>
#include "Router.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>


class Server
{
private:
    int port;
    int server_fd = -1;
    Router &router;
    std::queue<int> connections;
    std::mutex mut;
    std::condition_variable cv;
    std::vector<std::thread> workers;
    bool run{true};

    void handle_client(int client_fd);
    void worker();
    static void parse_request_line_and_query(const std::string &line, HttpRequest &req);

public:
    Server(int port, Router &router);
    ~Server();
    void start();
};