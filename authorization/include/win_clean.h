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

// Сохраняем состояние
#ifdef byte
#pragma message("Warning: byte is already defined before including Windows headers")
#undef byte
#endif

// Включаем минимальные Windows заголовки
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Очищаем после инклюдов если что-то осталось
#ifdef byte
#undef byte
#endif

#endif // _WIN32

#endif // WIN_CLEAN_H