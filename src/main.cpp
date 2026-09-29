#include "Router.hpp"
#include "Server.hpp"

int main() {
    Router app;

    app.get("/", [](HttpRequest &req, HttpResponse &res) {
        res.send("Welcome to the C++ Server!");
    });

    app.get("/users/:id", [](HttpRequest &req, HttpResponse &res) {
        std::string user_id = req.params["id"];
        res.json("{\"id\": \"" + user_id + "\", \"status\": \"active\"}");
    });

    app.post("/echo", [](HttpRequest &req, HttpResponse &res) {
        res.send("Received body: " + req.body);
    });

    Server server(6060, app);
    server.start();

    return 0;
}