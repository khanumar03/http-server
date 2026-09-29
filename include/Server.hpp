#pragma once
#include "Router.hpp"

class Server {
private:
    int port;
    int server_fd = -1;
    Router& router;

    void handle_client(int client_fd);
    static void parse_request_line_and_query(const std::string& line, HttpRequest& req);

public:
    Server(int port, Router& router);
    ~Server();
    void start();
};