#pragma once
#include <string>
#include <functional>

// Response returned by a route handler / the dispatcher.
struct HttpResponse {
    std::string body;
    std::string contentType = "text/html; charset=utf-8";
    int status = 200;
};

// Callback invoked for every incoming request: (method, path) -> response.
using HttpHandler = std::function<HttpResponse(const std::string& method, const std::string& path)>;

// Starts the server on the given port and blocks forever (accept() loop).
// Returns a non-empty error message if startup fails.
std::string startLocalServer(int port, HttpHandler handler);
