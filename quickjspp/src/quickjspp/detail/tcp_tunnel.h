#pragma once
#include <cstdint>
#include "net_def.h"
namespace qjs::detail
{
    struct TcpTunnel
    {
        socket_t socket;
        bool Read(uint8_t* buffer,size_t size)
        {
            size_t total_read = 0;
            while (total_read < size)
            {
                ssize_t bytes_read = recv(socket, reinterpret_cast<char*>(buffer) + total_read, size - total_read, 0);
                if (bytes_read <= 0)
                {
                    return false; // 读取失败或连接关闭
                }
                total_read += bytes_read;
            }
            return true; // 成功读取指定大小的数据
        }
        bool Write(const uint8_t* buffer, size_t size)
        {
            size_t total_written = 0;
            while (total_written < size)
            {
                ssize_t bytes_written = send(socket, reinterpret_cast<const char*>(buffer) + total_written, size - total_written, 0);
                if (bytes_written <= 0)
                {
                    return false; // 写入失败或连接关闭
                }
                total_written += bytes_written;
            }
            return true; // 成功写入指定大小的数据
        }
    };
}