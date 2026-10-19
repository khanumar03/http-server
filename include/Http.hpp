#pragma once
#include <string>
#include <unordered_map>

class Payload
{
public:
    std::string version = "HTTP/1.1";
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    void set_header(const std::string &key, const std::string &value);
    std::string get_header(const std::string &key) const;
};

class HttpRequest : public Payload
{
public:
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> params;

    HttpRequest() = default;
};

class HttpResponse : public Payload
{
private:
    int status = 200;

public:
    std::string status_text = "ok";
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> params;

    HttpResponse() = default;

    void set_status(int code, const std::string &text);
    void send(const std::string &data);
    void json(const std::string &json_str);
    std::string serialize() const;
};