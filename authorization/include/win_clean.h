// win_clean.h
#ifndef WIN_CLEAN_H
#define WIN_CLEAN_H

#ifdef _WIN32

// Блокируем стандартные определения перед включением
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Временно переименовываем byte на время инклюдов
#define byte windows_byte
#define _byte windows_byte_

// Включаем минимальные Windows заголовки
#include <windows.h>

// Немедленно отменяем переименование
#undef byte
#undef _byte

// Восстанавливаем для совместимости
typedef unsigned char byte;

// Подключаем специфичные для сети заголовки
#include <winsock2.h>
#include <ws2tcpip.h>

#endif // _WIN32

#endif // WIN_CLEAN_H