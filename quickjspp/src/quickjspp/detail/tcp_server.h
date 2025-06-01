#pragma once
#include <string>
#include <stdexcept>

#ifdef _WIN32
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  using socket_t = int;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
#endif

class TcpServer {
public:
    TcpServer() {

    }
    bool Start(uint16_t port, const std::string& host = "0.0.0.0")
    {

        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock == INVALID_SOCKET)
             return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));

        if (bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
             return false;

        if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR)
             return false;
        return true;
    }
    /**
     * @note call closesocket to close the socket after use
     * example:
     * if (closesocket(client) == SOCKET_ERROR)
     * {
     *     LogE("Failed to close client socket: {}", std::strerror(errno));
     * }
    */
    socket_t Accept() {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int len = sizeof(client_addr);
#else
        socklen_t len = sizeof(client_addr);
#endif
        socket_t client_sock = ::accept(listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (client_sock == INVALID_SOCKET)
            throw std::runtime_error("accept() failed");

        return client_sock;
    }

    void Close() {
        if (listen_sock != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(listen_sock);
            
#else
            ::close(listen_sock);
#endif
            listen_sock = INVALID_SOCKET;
        }
    }

    ~TcpServer() {
        Close();
    }

private:
    socket_t listen_sock;
};
