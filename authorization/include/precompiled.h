#ifndef PRECOMPILED_H
#define PRECOMPILED_H

// Решаем конфликт byte между Windows и C++17
#ifdef _WIN32
// Отключаем Windows byte definition
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Полностью отключаем Windows BYTE перед инклюдами
#define _NO_BYTE
#define byte win_byte_placeholder  // переименовываем Windows byte

// Добавляем эти определения для предотвращения конфликтов
#include <windows.h>  // должен быть первым

// После windows.h отменяем определение byte
#ifdef byte
#undef byte
#endif

// И переопределяем как unsigned char для совместимости
typedef unsigned char byte;

#endif  // _WIN32

#endif