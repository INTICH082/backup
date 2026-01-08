// win_compat.h
#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#ifdef _WIN32
// Отключаем всё лишнее перед включением Windows заголовков
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Временное переименование byte
#define byte windows_byte
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#undef byte

// Восстанавливаем оригинальное определение если нужно
#ifdef __cplusplus
#include <cstddef>
#endif

#endif // _WIN32

#endif // WIN_COMPAT_H