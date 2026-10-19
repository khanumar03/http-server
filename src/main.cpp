#include "Router.hpp"
#include "Server.hpp"

int main()
{
    Router app;

    app.get("/", [](HttpRequest &req, HttpResponse &res)
            {
        res.set_header("Content-Type", "text/plain");
        res.send("hello world"); });

    app.get("/users/:id", [](HttpRequest &req, HttpResponse &res)
            {
        std::string user_id = req.params["id"];
        res.json("{\"id\": \"" + user_id + "\", \"status\": \"active\"}"); });

    app.post("/echo", [](HttpRequest &req, HttpResponse &res)
             {
        res.set_status(201, "");
        res.set_header("Content-Type", "text/plain");
        res.send("Received body: " + req.body); });

    Server server(5000, app);
    server.start();

    return 0;
}