#ifndef PRECOMPILED_H
#define PRECOMPILED_H

// Решаем конфликт byte между Windows и C++17
#ifdef _WIN32

// Отключаем всё лишнее перед включением Windows заголовков
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Сохраняем оригинальное определение Windows BYTE где-нибудь
// но переименовываем byte чтобы избежать конфликта с C++17
#if defined(byte)
#undef byte
#endif

// Включаем Windows заголовки в правильном порядке
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// После инклюдов Windows заголовков проверяем и корректируем
// Windows определяет BYTE как unsigned char, а C++17 имеет std::byte
// Используем препроцессор для безопасного использования

#ifdef byte
#undef byte  // Убираем Windows byte если он есть
#endif

#endif // _WIN32

// Включаем стандартные заголовки C++
#include <cstddef>

// Определяем макрос для безопасного использования
#ifdef _WIN32
#define WINDOWS_BYTE unsigned char
#else
#define WINDOWS_BYTE unsigned char
#endif

#endif // PRECOMPILED_H