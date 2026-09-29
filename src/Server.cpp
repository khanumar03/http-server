#include "Server.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sstream>
#include <algorithm>

Server::Server(int port, Router& router) : port(port), router(router) {}

Server::~Server() {
    if (server_fd != -1) close(server_fd);
}

void Server::parse_request_line_and_query(const std::string& line, HttpRequest& req) {
    std::stringstream ss(line);
    std::string raw_url;
    ss >> req.method >> raw_url >> req.version;

    size_t qpos = raw_url.find('?');
    if (qpos == std::string::npos) {
        req.path = raw_url;
    } else {
        req.path = raw_url.substr(0, qpos);
        std::stringstream qs(raw_url.substr(qpos + 1));
        std::string pair;
        while (std::getline(qs, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                req.query[pair.substr(0, eq)] = pair.substr(eq + 1);
            } else if (!pair.empty()) {
                req.query[pair] = "";
            }
        }
    }
}

void Server::handle_client(int c) {
    char buffer[512];
    std::string data = "";
    ssize_t buf = 0;
    size_t hep = std::string::npos;

    // Phase 1: Read headers
    while ((hep = data.find("\r\n\r\n")) == std::string::npos) {
        buf = read(c, buffer, sizeof(buffer));
        if (buf <= 0) break;
        data.append(buffer, buf);
    }

    if (hep == std::string::npos) {
        close(c);
        return;
    }

    HttpRequest req;
    HttpResponse res;
    req.body = data.substr(hep + 4);

    // Phase 2: Parse header lines
    std::vector<std::string> raw_headers;
    size_t start = 0, end = 0;
    while ((end = data.find("\r\n", start)) != std::string::npos) {
        if (start == end) break;
        raw_headers.push_back(data.substr(start, end - start));
        start = end + 2;
    }

    if (!raw_headers.empty()) {
        parse_request_line_and_query(raw_headers[0], req);
    }

    size_t content_length = 0;
    for (size_t i = 1; i < raw_headers.size(); ++i) {
        size_t colon = raw_headers[i].find(':');
        if (colon != std::string::npos) {
            std::string k = raw_headers[i].substr(0, colon);
            std::string v = raw_headers[i].substr(raw_headers[i].find_first_not_of(" \t", colon + 1));
            req.set_header(k, v);
            if (k == "Content-Length") {
                try { content_length = std::stoul(v); } catch (...) { content_length = 0; }
            }
        }
    }

    // Phase 3: Drain remaining body bytes
    if (content_length > req.body.size()) {
        content_length -= req.body.size();
        while (content_length > 0) {
            size_t to_read = std::min((size_t)sizeof(buffer), content_length);
            buf = read(c, buffer, to_read);
            if (buf <= 0) break;
            req.body.append(buffer, buf);
            content_length -= buf;
        }
    }

    // Phase 4: Route and Respond
    router.handle(req, res);
    std::string out = res.serialize();
    send(c, out.data(), out.size(), 0);
    close(c);
}

void Server::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        sockaddr_in client{};
        socklen_t c_size = sizeof(client);
        int c = accept(server_fd, (struct sockaddr *)&client, &c_size);
        if (c == -1) continue;

        handle_client(c);
    }
}