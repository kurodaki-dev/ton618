#pragma once
#include <string>
#include <functional>

// A parsed incoming HTTP request, handed to the dispatcher (see
// Interpreter::registerBuiltinSys's sibling, the "serve"/"get"/"post"
// natives in src/Interpreter.cpp) so a .ton route handler can read the
// method, path, query string, and body of the request that reached it.
struct HttpRequest {
    std::string method;
    std::string path;   // without the query string
    std::string query;  // raw query string with no leading '?' (empty if none)
    std::string body;   // raw request body (POST/PUT/... payload)
};

// Response returned by a route handler / the dispatcher.
struct HttpResponse {
    std::string body;
    std::string contentType = "text/html; charset=utf-8";
    int status = 200;
};

// Callback invoked for every incoming request.
using HttpHandler = std::function<HttpResponse(const HttpRequest& request)>;

// Starts the server on the given port and blocks forever (accept() loop).
// Returns a non-empty error message if startup fails.
std::string startLocalServer(int port, HttpHandler handler);
