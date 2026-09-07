# Archetyped: SQL База данных

Сервис SQLDB предоставляет простой интерфейс ключ-значение и SQL на базе SQLite.

## Обзор

Каждый домен SQLDB представляет собой изолированное соединение с базой данных SQLite.

## Методы класса SQLDB

### Инициализация

#### `SQLDB(std::string name)`
Регистрирует новый домен базы данных SQL.

#### `open(const char* dbPath)`
Открывает файл базы данных по указанному пути. Если файл не существует, он будет создан.

### Операции

#### `execute(const char* sql)`
Выполняет произвольную SQL-команду.
-   **Пример**: `db.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT);");`

#### `setString(const char* key, const char* value)`
Вспомогательный метод для сохранения строкового значения, связанного с ключом, в таблице по умолчанию `kv_store`.

#### `exists(const char* key)`
Проверяет, существует ли ключ в таблице `kv_store`.

#### `close()`
Закрывает соединение с базой данных.

## Пример использования

```cpp
#include "Engine/SQLDB.h"

void DatabaseExample() {
    SQLDB db("MySettings");
    db.open("settings.db");

    // Стиль Ключ-Значение
    db.setString("resolution", "1920x1080");
    if (db.exists("resolution")) {
        // ...
    }

    // Чистый SQL
    db.execute("INSERT INTO kv_store (key, value) VALUES ('version', '1.0');");

    db.close();
}
```
