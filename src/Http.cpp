#include "Http.hpp"

void Payload::set_header(const std::string &key, const std::string &val) {
    headers[key] = val;
}

std::string Payload::get_header(const std::string &key) const {
    auto it = headers.find(key);
    return (it != headers.end()) ? it->second : "";
}

void HttpResponse::set_status(int code, const std::string& text) {
    status = code;
    if (!text.empty()) status_text = text;
    else if (code == 200) status_text = "OK";
    else if (code == 201) status_text = "Created";
    else if (code == 404) status_text = "Not Found";
    else if (code == 500) status_text = "Internal Server Error";
}

void HttpResponse::send(const std::string& text) {
    body = text;
}

void HttpResponse::json(const std::string& json_str) {
    body = json_str;
    set_header("Content-Type", "application/json");
}

std::string HttpResponse::serialize() const {
    std::string res = version + " " + std::to_string(status) + " " + status_text + "\r\n";
    for (const auto& [k, v] : headers) {
        res += k + ": " + v + "\r\n";
    }
    res += "\r\n";
    res += body;
    return res;
}