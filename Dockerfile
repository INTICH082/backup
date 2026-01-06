FROM ubuntu:22.04

# Устанавливаем таймзону
ENV TZ=Europe/Moscow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Устанавливаем зависимости для SQLite
RUN apt-get update && apt-get install -y \
    g++ \
    curl \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Создаем рабочую директорию
WORKDIR /app

# Копируем ВСЕ файлы проекта
COPY . .

# Переходим в папку модуля авторизации
WORKDIR /app/authorization

# Компилируем проект с SQLite
RUN g++ -c database.cpp -std=c++11 \
    && g++ -c auth.cpp -std=c++11 \
    && g++ -c server.cpp -std=c++11 \
    && g++ -c main.cpp -std=c++11 \
    && g++ database.o auth.o server.o main.o -o auth_server -lcurl -lsqlite3 -lpthread

# Открываем порт
EXPOSE 8081

# Запускаем сервер
CMD ["./auth_server"]