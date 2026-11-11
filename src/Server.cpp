#include "Server.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sstream>
#include <algorithm>
#include <thread>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <cstring>
#include <iterator>

Server::Server(int port, Router &router) : port(port), router(router)
{
    unsigned int num_workers = std::thread::hardware_concurrency();
    if (num_workers == 0)
        num_workers = 4;

    workers.reserve(num_workers);
    for (int i = 0; i < num_workers; i++)
    {
        workers.emplace_back(&Server::worker, this);
    }
}

Server::~Server()
{
    {
        std::lock_guard<std::mutex> lock(mut);
        run = false;
    }

    cv.notify_all();

    for (std::thread &w : workers)
    {
        if (w.joinable())
            w.join();
    }
    if (server_fd != -1)
        close(server_fd);
}

void Server::worker()
{
    while (run)
    {
        int fd = -1;

        {
            std::unique_lock<std::mutex> lock(mut);
            cv.wait(lock, [this]()
                    { return !connections.empty() || !run; });

            if (!connections.empty())
            {

                fd = connections.front();
                connections.pop();
            }
        }

        if (fd != -1)
            handle_client(fd);
    }
}

void Server::parse_request_line_and_query(const std::string &line, HttpRequest &req)
{
    std::stringstream ss(line);
    std::string path;
    ss >> req.method >> path >> req.version;

    size_t qp = path.find('?');

    if (qp == std::string::npos)
        req.path = path;
    else
    {
        req.path = path.substr(0, qp);
        std::stringstream t(path.substr(qp + 1));
        std::string pair;

        while (std::getline(t, pair, '&'))
        {
            if (pair.empty())
                continue;

            std::stringstream q(pair);
            std::string key;
            std::string val;
            std::getline(q, key, '=');
            std::getline(q, val);
            req.query[key] = val;
        }
    }
}

void Server::handle_client(int client_fd)
{
    char buffer[4096];
    std::string unparsed_data;

    while (true)
    {
        size_t hep = unparsed_data.find("\r\n\r\n");

        while (hep == std::string::npos)
        {
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0)
            {
                close(client_fd);
                return;
            }
            unparsed_data.append(buffer, bytes_read);
            hep = unparsed_data.find("\r\n\r\n");
        }

        std::string header_section = unparsed_data.substr(0, hep);
        unparsed_data.erase(0, hep + 4);

        HttpRequest req;
        HttpResponse res;

        std::vector<std::string> raw_headers;
        size_t start = 0, end = 0;
        while ((end = header_section.find("\r\n", start)) != std::string::npos)
        {
            if (start == end)
                break;
            raw_headers.push_back(header_section.substr(start, end - start));
            start = end + 2;
        }
        if (start < header_section.size())
        {
            raw_headers.push_back(header_section.substr(start));
        }

        if (raw_headers.empty())
        {
            close(client_fd);
            return;
        }

        parse_request_line_and_query(raw_headers[0], req);

        size_t content_length = 0;
        std::string Connection = "";

        for (size_t i = 1; i < raw_headers.size(); i++)
        {
            size_t colon = raw_headers[i].find(':');
            if (colon != std::string::npos)
            {
                std::string k = raw_headers[i].substr(0, colon);
                std::string v = raw_headers[i].substr(raw_headers[i].find_first_not_of(" \t", colon + 1));
                req.set_header(k, v);

                if (k == "Content-Length")
                {
                    try
                    {
                        content_length = std::stoul(v);
                    }
                    catch (...)
                    {
                        content_length = 0;
                    }
                }
                else if (k == "Connection")
                {
                    Connection = v;
                }
            }
        }

        while (unparsed_data.size() < content_length)
        {
            size_t needed = content_length - unparsed_data.size();
            size_t to_read = std::min((size_t)sizeof(buffer), needed);
            ssize_t bytes_read = read(client_fd, buffer, to_read);
            if (bytes_read <= 0)
            {
                close(client_fd);
                return;
            }
            unparsed_data.append(buffer, bytes_read);
        }

        req.body = unparsed_data.substr(0, content_length);
        unparsed_data.erase(0, content_length);

        std::string conn_lower = Connection;
        std::transform(conn_lower.begin(), conn_lower.end(), conn_lower.begin(), ::tolower);

        bool keep_alive = true;
        if (conn_lower == "close" || (req.version == "HTTP/1.0" && conn_lower != "keep-alive"))
            keep_alive = false;

        router.handle(req, res);
        res.set_header("Content-Length", std::to_string(res.body.size()));
        res.set_header("Connection", keep_alive ? "keep-alive" : "close");
        std::string out = res.serialize();
        send(client_fd, out.data(), out.size(), 0);

        if (!keep_alive)
            break;
    }
    close(client_fd);
}

void Server::start()
{
    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, SOMAXCONN);

    int epc = epoll_create1(0);
    struct epoll_event epe{};
    epe.events = EPOLLIN;
    epe.data.fd = server_fd;
    epoll_ctl(epc, EPOLL_CTL_ADD, server_fd, &epe);

    std::cout << "Server listening on port " << port << std::endl;

    epoll_event events[1024];

    while (true)
    {
        int n = epoll_wait(epc, events, std::size(events), 1000);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "epoll_wait failed: " << strerror(errno);
            break;
        }

        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {

                while (1)
                {
                    int client_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);

                    if (client_fd >= 0)
                    {
                        int f = 1;
                        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &f, sizeof(f));
                        struct epoll_event c_epe{};
                        c_epe.events = EPOLLIN;
                        c_epe.data.fd = client_fd;
                        epoll_ctl(epc, EPOLL_CTL_ADD, client_fd, &c_epe);
                        std::cout << "new connection" << std::endl;
                        handle_client(c_epe.data.fd);
                        // close(client_fd);
                        continue;
                    }
                    else if (errno == EAGAIN)
                    {
                        // std::cout<< "queue is empty" << std::endl;
                        break;
                    }
                    else if (errno == EINTR)
                    {
                        std::cout << "Interrupted" << std::endl;
                        continue;
                    }
                    else if (errno == ECONNABORTED)
                    {
                        std::cout << "client close the connection" << std::endl;
                        continue;
                    }
                    else if (errno == EMFILE)
                    {
                        std::cout << "out of fd" << std::endl;
                        break;
                    }
                    else
                    {
                        std::cerr << "error: " << strerror(errno) << std::endl;
                        break;
                    }
                }
            }
        }
    }

    // while (true)
    // {
    //     sockaddr_in client_sock{};
    //     socklen_t size_c = sizeof(client_sock);
    //     int client_fd = accept(server_fd, (struct sockaddr *)&client_sock, &size_c);
    //     if (client_fd == -1)
    //         continue;
    //     int f = 1;
    //     setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &f, sizeof(f));
    //     struct timeval tv;
    //     tv.tv_sec = 0;
    //     tv.tv_usec = 500;
    //     setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    //     {
    //         std::lock_guard<std::mutex> lock(mut);
    //         connections.push(client_fd);
    //     }

    //     cv.notify_one();
    // }
}