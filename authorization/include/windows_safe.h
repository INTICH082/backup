// windows_safe.h
#ifndef WINDOWS_SAFE_H
#define WINDOWS_SAFE_H

#ifdef _WIN32

// Сохраняем текущее состояние
#pragma push_macro("byte")
#pragma push_macro("BYTE")

// Убираем эти макросы перед включением Windows заголовков
#ifdef byte
#undef byte
#endif

#ifdef BYTE
#undef BYTE
#endif

// Включаем Windows заголовки в правильном порядке
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Восстанавливаем оригинальные макросы
#pragma pop_macro("byte")
#pragma pop_macro("BYTE")

#endif // _WIN32

#endif // WINDOWS_SAFE_H