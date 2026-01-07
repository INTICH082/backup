# Модуль авторизации (Auth Module)

## Для участников проекта

Этот модуль предоставляет систему авторизации через GitHub OAuth. Все данные хранятся в облачной MongoDB Atlas.

## Как использовать API

### 1. Авторизация пользователя
1. Перенаправьте пользователя на: `GET /auth/github`
2. После авторизации GitHub перенаправит на callback
3. Получите JWT токен

### 2. Валидация токенов
```bash
# Проверить JWT токен
GET /auth/validate?token=<jwt_token>

# Обновить токены
POST /auth/refresh?refresh_token=<refresh_token>&user_id=<user_id>

# Выйти из системы
POST /auth/logout?token=<jwt_token>&user_id=<user_id>