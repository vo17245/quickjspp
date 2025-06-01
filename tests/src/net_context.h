#pragma once

#include <string>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif




struct NetContext
{
    static bool Init()
    {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            return false;
#endif
        return true;
    }
    static void Cleanup()
    {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};