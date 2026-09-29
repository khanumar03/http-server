#include "Router.hpp"
#include <sstream>

void Router::get(const std::string &path, Handler handler) {
    routes.push_back({"GET", path, handler});
}

void Router::post(const std::string &path, Handler handler) {
    routes.push_back({"POST", path, handler});
}

std::vector<std::string> Router::split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        if (!seg.empty()) parts.push_back(seg);
    }
    return parts;
}

bool Router::match(const std::string& pattern, const std::string& actual_path, 
                   std::unordered_map<std::string, std::string>& params) {
    auto pattern_parts = split_path(pattern);
    auto actual_parts = split_path(actual_path);

    if (pattern_parts.size() != actual_parts.size()) return false;

    std::unordered_map<std::string, std::string> matched_params;

    for (size_t i = 0; i < pattern_parts.size(); ++i) {
        if (pattern_parts[i].rfind(":", 0) == 0) {
            matched_params[pattern_parts[i].substr(1)] = actual_parts[i];
        } else if (pattern_parts[i] != actual_parts[i]) {
            return false;
        }
    }

    params = std::move(matched_params);
    return true;
}

void Router::handle(HttpRequest &req, HttpResponse &res) {
    for (const auto &route : routes) {
        if (route.method == req.method && match(route.path, req.path, req.params)) {
            route.handler(req, res);
            return;
        }
    }

    res.set_status(404, "Not Found");
    res.send("Cannot " + req.method + " " + req.path);
}