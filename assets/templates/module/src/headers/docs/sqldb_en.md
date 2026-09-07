# Archetyped: SQL Database

The SQLDB service provides a simple key-value and SQL interface powered by SQLite.

## Overview

Each SQLDB domain represents an isolated SQLite database connection.

## SQLDB Class Methods

### Initialization

#### `SQLDB(std::string name)`
Registers a new SQL database domain.

#### `open(const char* dbPath)`
Opens a database file at the specified path. If the file doesn't exist, it will be created.

### Operations

#### `execute(const char* sql)`
Executes an arbitrary SQL command.
-   **Example**: `db.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT);");`

#### `setString(const char* key, const char* value)`
A helper method to store a string value associated with a key in a default `kv_store` table.

#### `exists(const char* key)`
Checks if a key exists in the `kv_store` table.

#### `close()`
Closes the database connection.

## Usage Example

```cpp
#include "Engine/SQLDB.h"

void DatabaseExample() {
    SQLDB db("MySettings");
    db.open("settings.db");

    // Key-Value style
    db.setString("resolution", "1920x1080");
    if (db.exists("resolution")) {
        // ...
    }

    // Pure SQL
    db.execute("INSERT INTO kv_store (key, value) VALUES ('version', '1.0');");

    db.close();
}
```
