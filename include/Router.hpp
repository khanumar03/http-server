#pragma once
#include <vector>
#include <functional>
#include "Http.hpp"

using Handler = std::function<void(HttpRequest &, HttpResponse &)>;

struct Route {
    std::string method;
    std::string path;
    Handler handler;
};

class Router {
public:
    std::vector<Route> routes;

    Router() = default;
    void get(const std::string &path, Handler handler);
    void post(const std::string &path, Handler handler);
    void handle(HttpRequest &req, HttpResponse &res);

private:
    static std::vector<std::string> split_path(const std::string& path);
    static bool match(const std::string& pattern, const std::string& actual_path, 
                      std::unordered_map<std::string, std::string>& params);
};