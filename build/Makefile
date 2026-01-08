CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./include
LDFLAGS = -lcurl -lws2_32

# Если nlohmann/json не установлена глобально, скачайте одним файлом:
# https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
# Положите в include/nlohmann/json.hpp

SOURCES = src/main.cpp \
          src/AuthServer.cpp \
          src/Config.cpp \
          src/SimpleDB.cpp \
          src/GitHubOAuth.cpp \
          src/JWT.cpp

OBJECTS = $(SOURCES:.cpp=.o)
TARGET = auth_module.exe

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	del /Q src\*.o $(TARGET) 2>nul || true

run: $(TARGET)
	.\$(TARGET)

.PHONY: all clean run