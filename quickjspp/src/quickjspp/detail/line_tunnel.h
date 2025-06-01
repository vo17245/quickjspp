#pragma once
#include <functional>
#include <string_view>
namespace qjs::detail
{
    struct LineTunnel
    {
        std::function<bool(uint8_t&)> readByte;
        std::function<bool(const uint8_t*, size_t)> writeBytes;
        bool WriteLine(std::string_view line)//line的结尾必须是\n
        {
           return writeBytes(reinterpret_cast<const uint8_t*>(line.data()), line.size());
        }
        bool ReadLine(std::string& line)
        {
            line.clear();
            uint8_t byte;
            bool res= readByte(byte);
            if(!res)
            {
                return false;
            }
            while (byte != '\n') // 读取直到换行符
            {
                line.push_back(static_cast<char>(byte));
                res = readByte(byte);
                if (!res)
                {
                    return false; // 读取失败
                }
            }
            return true; // 成功读取一行
        }
    };
}