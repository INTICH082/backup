cmake_minimum_required(VERSION 3.10)
project(AuthModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Для Windows
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    set(WIN32_LIBS ws2_32)
endif()

# Ищем CURL
find_package(CURL REQUIRED)

# Включаем директории
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann
    ${CURL_INCLUDE_DIRS}
)

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/AuthServer.cpp
    src/Config.cpp
    src/SimpleDB.cpp
    src/GitHubOAuth.cpp
    src/JWT.cpp
)

# Исполняемый файл
add_executable(auth_module ${SOURCES})

# Линковка библиотек
target_link_libraries(auth_module
    ${CURL_LIBRARIES}
    ${WIN32_LIBS}
)

# Копируем конфиг файл
configure_file(config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)