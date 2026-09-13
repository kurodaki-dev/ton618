#include "LocalServer.hpp"
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iostream>

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

        char buffer[8192] = {0};
#ifdef _WIN32
        int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
#else
        ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
#endif
        std::string method = "GET", path = "/";
        if (bytesRead > 0) {
            std::string request(buffer);
            std::istringstream reqStream(request);
            reqStream >> method >> path;
        }

        // Strip query string for route matching (?a=b)
        std::string cleanPath = path;
        auto qpos = cleanPath.find('?');
        if (qpos != std::string::npos) cleanPath = cleanPath.substr(0, qpos);

        HttpResponse resp;
        try {
            resp = handler(method, cleanPath);
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
