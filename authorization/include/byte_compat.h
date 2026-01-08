// byte_compat.h - совместимость с byte для Windows/C++
#ifndef BYTE_COMPAT_H
#define BYTE_COMPAT_H

#ifdef _WIN32
// Защита от конфликта std::byte и Windows byte
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Сохраняем оригинальное определение byte
#ifdef byte
#undef byte
#define byte windows_byte  // Переименовываем
#endif

// Включаем Windows заголовки
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Восстанавливаем возможность использования std::byte
#ifdef byte
#undef byte
#endif

// Для std::byte
#include <cstddef>

// Создаем алиас для Windows byte если нужно
namespace windows_compat {
    typedef unsigned char windows_byte;
}

#else
// Для не-Windows систем
#include <cstddef>
#endif // _WIN32

#endif // BYTE_COMPAT_H