#pragma once
#include <string>
#include <vector>
#include <utility>

// ============================================================================
// HttpClient.hpp — a minimal, blocking, dependency-free HTTP client (raw
// sockets, same spirit as LocalServer.hpp on the server side).
//
// Deliberately limited: **http:// only, no TLS** (there is no bundled SSL
// library to keep the interpreter dependency-free), no redirects, no chunked
// transfer-encoding decoding, no keep-alive. It's enough for talking to local
// APIs, other TON618 servers, or any plain-http endpoint from a script — see
// the `ton.requests` built-in module (Interpreter::registerBuiltinRequests
// in src/Interpreter.cpp) for how scripts reach this.
// ============================================================================

struct HttpClientResponse {
    bool ok = false;         // true if a response was received and parsed at all
    int status = 0;          // HTTP status code (0 if the request never got a response)
    std::string body;
    std::string error;       // non-empty explains why ok is false
};

// Performs a single HTTP request. `url` must start with "http://". `body` is
// sent as the request body for methods like POST (ignored for GET).
HttpClientResponse httpRequest(const std::string& method, const std::string& url, const std::string& body);
