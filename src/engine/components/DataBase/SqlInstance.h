#pragma once

#include <string>
#include <vector>

struct sqlite3;

class SqlInstance {
public:
    SqlInstance() = default;
    ~SqlInstance();

    SqlInstance(const SqlInstance&) = delete;
    SqlInstance& operator=(const SqlInstance&) = delete;
    

    bool open(const char* dbPath);
    void close();

    bool setString(const char* key, const char* value) noexcept;

    size_t getString(const char* key, char* outBuffer, size_t bufferSize) noexcept;

    bool exists(const char* key) noexcept;

    bool execute(const char* sql) noexcept;

    bool isOpen() const { return _db != nullptr; }

private:
    sqlite3* _db = nullptr;
};