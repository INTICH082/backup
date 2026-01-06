#include "database.h"
#include "config.h"
#include <sqlite3.h>
#include <iostream>

using namespace std;

static sqlite3* db = nullptr;

bool Database::connect() {
    if (db) return true;
    
    // Открываем файл БД (создастся автоматически)
    int rc = sqlite3_open("auth.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "❌ Ошибка открытия БД: " << sqlite3_errmsg(db) << endl;
        return false;
    }
    
    // Включаем поддержку внешних ключей
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    
    // Создаем таблицу users если её нет
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            login TEXT UNIQUE NOT NULL,
            password TEXT,
            fullname TEXT,
            email TEXT,
            github_id TEXT UNIQUE,
            telegram_id INTEGER UNIQUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "❌ Ошибка создания таблицы: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    cout << "✅ Подключено к SQLite БД (auth.db)" << endl;
    return true;
}

void Database::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

int Database::getUserByLogin(const string& login) {
    if (!db) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM users WHERE login = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    
    int user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return user_id;
}

pair<int, string> Database::getUserWithPasswordHash(const string& login) {
    pair<int, string> result = {0, ""};
    if (!db) return result;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, password FROM users WHERE login = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }
    
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result.first = sqlite3_column_int(stmt, 0);
        const char* pass = (const char*)sqlite3_column_text(stmt, 1);
        if (pass) result.second = string(pass);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

int Database::createUserWithPassword(const string& login, const string& password_hash,
                                    const string& name, const string& email) {
    if (!db) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (login, password, fullname, email) VALUES (?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    int user_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return user_id;
}

int Database::createGitHubUser(const string& login, const string& name,
                              const string& email, const string& github_id) {
    if (!db) return 0;
    
    // Сначала проверяем, нет ли уже пользователя с таким github_id
    int existing_id = getUserByGithubId(github_id);
    if (existing_id != 0) {
        return existing_id;
    }
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (login, password, fullname, email, github_id) VALUES (?, '', ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, github_id.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    int user_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return user_id;
}

int Database::createTelegramUser(const string& login, const string& name,
                                const string& email, long long telegram_id) {
    if (!db) return 0;
    
    // Сначала проверяем, нет ли уже пользователя с таким telegram_id
    int existing_id = getUserByTelegramId(telegram_id);
    if (existing_id != 0) {
        return existing_id;
    }
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (login, password, fullname, email, telegram_id) VALUES (?, '', ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, telegram_id);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    int user_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return user_id;
}

int Database::getUserByGithubId(const string& github_id) {
    if (!db) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM users WHERE github_id = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, github_id.c_str(), -1, SQLITE_STATIC);
    
    int user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return user_id;
}

int Database::getUserByTelegramId(long long telegram_id) {
    if (!db) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM users WHERE telegram_id = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int64(stmt, 1, telegram_id);
    
    int user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return user_id;
}

bool Database::userExists(int user_id) {
    if (!db) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM users WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    
    sqlite3_finalize(stmt);
    return exists;
}