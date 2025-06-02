#pragma once


#ifdef _WIN32
#include <winsock2.h>
#endif
#include <cstdint>
namespace qjs::detail
{

#ifdef _WIN32
using socket_t = SOCKET;
using ssize_t =int64_t;
#else
using socket_t = int;
const inline constexpr int INVALID_SOCKET = -1;
const inline constexpr int SOCKET_ERROR = -1;
#endif


}