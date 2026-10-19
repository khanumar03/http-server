#include "Router.hpp"
#include <sstream>
#include <iostream>

void Router::get(const std::string &path, Handler handler)
{
    routes.push_back({"GET", path, handler});
}

void Router::post(const std::string &path, Handler handler)
{
    routes.push_back({"POST", path, handler});
}

std::vector<std::string> Router::split_path(const std::string &path)
{
    std::vector<std::string> sub_paths;
    std::stringstream ss(path);
    std::string buf;

    while (std::getline(ss, buf, '/'))
    {
        if (!buf.empty()) {
            sub_paths.push_back(buf);
        }
    }

    return sub_paths;
}

bool Router::match(const std::string &path, const std::string &actual_path, std::unordered_map<std::string, std::string> &params)
{
    std::vector<std::string> client_sub_paths = split_path(path);
    std::vector<std::string> server_sub_paths = split_path(actual_path);

    if (client_sub_paths.size() != server_sub_paths.size())
        return false;

    int n = client_sub_paths.size();
    std::unordered_map<std::string, std::string> _params;

    for (int i = 0; i < n; i++)
    {
        if (!server_sub_paths[i].empty() && server_sub_paths[i][0] == ':')
        {
            std::string key = server_sub_paths[i].substr(1);
            _params[key] = client_sub_paths[i];
        }
        else if (client_sub_paths[i] != server_sub_paths[i])
        {
            return false;
        }
    }

    params = std::move(_params);
    return true;
}

void Router::handle(HttpRequest &req, HttpResponse &res)
{
    for (auto &route : routes)
    {
        if (req.method == route.method && match(req.path, route.path, req.params))
        {
            route.handler(req, res);
            return;
        }
    }

    res.set_status(404, "Not found");
    res.send("something went wrong");
}