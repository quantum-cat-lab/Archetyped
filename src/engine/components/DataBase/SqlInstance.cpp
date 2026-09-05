#include "SqlInstance.h"
#include <sqlite3.h>
#include <iostream>
#include <cstring>

SqlInstance::~SqlInstance() {
    close();
}

bool SqlInstance::open(const char* dbPath) {
    if (sqlite3_open(dbPath, &_db) != SQLITE_OK) {
        std::cerr << "[SqlInstance] Error opening: " << sqlite3_errmsg(_db) << std::endl;
        _db = nullptr;
        return false;
    }

    const char* sql = "CREATE TABLE IF NOT EXISTS registry (key TEXT PRIMARY KEY, val TEXT);";
    return execute(sql);
}

void SqlInstance::close() {
    if (_db) {
        sqlite3_close(_db);
        _db = nullptr;
    }
}

bool SqlInstance::setString(const char* key, const char* value) noexcept {
    if (!_db) return false;

    const char* sql = "INSERT OR REPLACE INTO registry (key, val) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

size_t SqlInstance::getString(const char* key, char* outBuffer, size_t bufferSize) noexcept {
    if (!_db || !outBuffer || bufferSize == 0) return 0;

    const char* sql = "SELECT val FROM registry WHERE key = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    size_t copied = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (val) {
            size_t len = std::strlen(val);
            copied = (len < bufferSize) ? len : bufferSize - 1;
            std::memcpy(outBuffer, val, copied);
            outBuffer[copied] = '\0';
        }
    }

    sqlite3_finalize(stmt);
    return copied;
}

bool SqlInstance::exists(const char* key) noexcept {
    if (!_db) return false;

    const char* sql = "SELECT 1 FROM registry WHERE key = ? LIMIT 1;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

bool SqlInstance::execute(const char* sql) noexcept {
    if (!_db) return false;
    return sqlite3_exec(_db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}