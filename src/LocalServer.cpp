#include "LocalServer.hpp"
#include <cstring>
#include <cstdint>
#include <cctype>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <arpa/inet.h>
#endif

namespace {

int recvSome(int fd, char* buf, int len) {
#ifdef _WIN32
    return recv(fd, buf, len, 0);
#else
    return (int)read(fd, buf, (size_t)len);
#endif
}

// A request body larger than this is truncated rather than exhausting memory
// on a bad/malicious Content-Length — generous for the kind of local JSON
// APIs this server is meant for.
constexpr size_t MAX_BODY_BYTES = 10 * 1024 * 1024;

size_t parseContentLength(const std::string& headerBlock) {
    std::istringstream hs(headerBlock);
    std::string line;
    std::getline(hs, line); // the request line ("GET /path HTTP/1.1"), not a header
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        for (auto& c : key) c = (char)std::tolower((unsigned char)c);
        if (key == "content-length") {
            try { return (size_t)std::stoul(line.substr(colon + 1)); } catch (...) { return 0; }
        }
    }
    return 0;
}

} // namespace

std::string startLocalServer(int port, HttpHandler handler) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "Failed to initialize Winsock.";
    }
#endif

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) return "Could not create socket.";

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        return "Could not bind to port " + std::to_string(port) + " (already in use?).";
    }
    if (listen(serverFd, 16) < 0) {
        return "listen() failed on port " + std::to_string(port) + ".";
    }

    std::cout << "TON618 server running at http://localhost:" << port << "/ (Ctrl+C to stop)\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) continue;

        // Read until the header/body separator shows up (bailing out past a
        // generous cap in case a client never sends one).
        std::string raw;
        char buffer[8192];
        size_t headerEnd = std::string::npos;
        while (headerEnd == std::string::npos) {
            int n = recvSome(clientFd, buffer, sizeof(buffer));
            if (n <= 0) break;
            raw.append(buffer, (size_t)n);
            headerEnd = raw.find("\r\n\r\n");
            if (headerEnd == std::string::npos && raw.size() > 65536) break;
        }

        std::string method = "GET", path = "/";
        std::string headerBlock = (headerEnd != std::string::npos) ? raw.substr(0, headerEnd) : raw;
        {
            std::istringstream reqLine(headerBlock);
            reqLine >> method >> path;
        }

        // Keep reading past the headers until the full body (per
        // Content-Length) has arrived.
        std::string body = (headerEnd != std::string::npos) ? raw.substr(headerEnd + 4) : "";
        size_t contentLength = std::min(parseContentLength(headerBlock), MAX_BODY_BYTES);
        while (body.size() < contentLength) {
            int n = recvSome(clientFd, buffer, sizeof(buffer));
            if (n <= 0) break;
            body.append(buffer, (size_t)n);
        }
        if (body.size() > contentLength) body.resize(contentLength);

        // Split the query string off the path for route matching.
        std::string cleanPath = path, query;
        auto qpos = cleanPath.find('?');
        if (qpos != std::string::npos) {
            query = cleanPath.substr(qpos + 1);
            cleanPath = cleanPath.substr(0, qpos);
        }

        HttpResponse resp;
        try {
            resp = handler(HttpRequest{method, cleanPath, query, body});
        } catch (std::exception& e) {
            resp.status = 500;
            resp.body = std::string("Server error: ") + e.what();
            resp.contentType = "text/plain; charset=utf-8";
        }

        const char* statusText = (resp.status == 404) ? "Not Found" : (resp.status == 500) ? "Internal Server Error" : "OK";

        std::ostringstream response;
        response << "HTTP/1.1 " << resp.status << " " << statusText << "\r\n"
                 << "Content-Type: " << resp.contentType << "\r\n"
                 << "Content-Length: " << resp.body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << resp.body;
        std::string responseStr = response.str();

#ifdef _WIN32
        send(clientFd, responseStr.c_str(), (int)responseStr.size(), 0);
        closesocket(clientFd);
#else
        ssize_t written = write(clientFd, responseStr.c_str(), responseStr.size());
        (void)written;
        close(clientFd);
#endif
    }

    return "";
}
