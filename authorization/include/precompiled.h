#ifndef PRECOMPILED_H
#define PRECOMPILED_H

// Для Windows: предотвращаем конфликт с Windows byte
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Базовые заголовки C++
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <thread>
#include <algorithm>
#include <regex>
#include <memory>
#include <cstdlib>
#include <unordered_map>
#include <random>

// Используем локальную копию json.hpp
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

#endif // PRECOMPILED_H