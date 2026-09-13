#include "HttpClient.hpp"
#include <cstring>
#include <sstream>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
#endif

namespace {

// Splits "http://host:port/path?query" into its pieces. Only the "http"
// scheme is supported (see HttpClient.hpp for why). Defaults: port 80, path "/".
struct ParsedUrl {
    std::string host;
    int port = 80;
    std::string path = "/";
    bool valid = false;
};

ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl result;
    const std::string prefix = "http://";
    if (url.compare(0, prefix.size(), prefix) != 0) return result; // invalid: leaves valid=false

    std::string rest = url.substr(prefix.size());
    size_t slashPos = rest.find('/');
    std::string hostPort = (slashPos == std::string::npos) ? rest : rest.substr(0, slashPos);
    result.path = (slashPos == std::string::npos) ? "/" : rest.substr(slashPos);
    if (result.path.empty()) result.path = "/";

    size_t colonPos = hostPort.find(':');
    if (colonPos == std::string::npos) {
        result.host = hostPort;
    } else {
        result.host = hostPort.substr(0, colonPos);
        try { result.port = std::stoi(hostPort.substr(colonPos + 1)); } catch (...) { return result; }
    }
    if (result.host.empty()) return result;
    result.valid = true;
    return result;
}

} // namespace

HttpClientResponse httpRequest(const std::string& method, const std::string& url, const std::string& body) {
    HttpClientResponse resp;

    ParsedUrl parsed = parseUrl(url);
    if (!parsed.valid) {
        resp.error = "invalid URL (only 'http://host[:port]/path' is supported): " + url;
        return resp;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        resp.error = "failed to initialize Winsock.";
        return resp;
    }
#endif

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* addrResult = nullptr;
    std::string portStr = std::to_string(parsed.port);
    if (getaddrinfo(parsed.host.c_str(), portStr.c_str(), &hints, &addrResult) != 0 || !addrResult) {
        resp.error = "could not resolve host '" + parsed.host + "'.";
        return resp;
    }

    int sock = (int)socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(addrResult);
        resp.error = "could not create a socket.";
        return resp;
    }

    if (connect(sock, addrResult->ai_addr, (int)addrResult->ai_addrlen) < 0) {
        freeaddrinfo(addrResult);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        resp.error = "could not connect to " + parsed.host + ":" + portStr + ".";
        return resp;
    }
    freeaddrinfo(addrResult);

    std::ostringstream request;
    request << method << " " << parsed.path << " HTTP/1.1\r\n"
            << "Host: " << parsed.host << "\r\n"
            << "User-Agent: ton618\r\n"
            << "Connection: close\r\n";
    if (!body.empty()) {
        request << "Content-Type: application/json\r\n"
                << "Content-Length: " << body.size() << "\r\n";
    }
    request << "\r\n" << body;
    std::string requestStr = request.str();

#ifdef _WIN32
    send(sock, requestStr.c_str(), (int)requestStr.size(), 0);
#else
    ssize_t written = write(sock, requestStr.c_str(), requestStr.size());
    (void)written;
#endif

    std::string raw;
    char buffer[8192];
    while (true) {
#ifdef _WIN32
        int n = recv(sock, buffer, sizeof(buffer), 0);
#else
        ssize_t n = read(sock, buffer, sizeof(buffer));
#endif
        if (n <= 0) break;
        raw.append(buffer, (size_t)n);
    }
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        resp.error = "received an empty or malformed response.";
        return resp;
    }
    std::string statusLine = raw.substr(0, raw.find("\r\n"));
    // "HTTP/1.1 200 OK" -> pull out the number between the two spaces.
    size_t firstSpace = statusLine.find(' ');
    if (firstSpace != std::string::npos) {
        try { resp.status = std::stoi(statusLine.substr(firstSpace + 1)); } catch (...) { resp.status = 0; }
    }
    resp.body = raw.substr(headerEnd + 4);
    resp.ok = resp.status > 0;
    return resp;
}
