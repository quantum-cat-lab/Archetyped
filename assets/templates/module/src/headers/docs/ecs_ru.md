# Archetyped: Entity-Component-System (ECS)

Сервис ECS предоставляет способ управления сущностями и их компонентами высокопроизводительным и ориентированным на данные способом.

## Обзор

-   **Entity (Сущность)**: Простой идентификатор, представляющий объект.
-   **Component (Компонент)**: Чистые данные, связанные с сущностью.
-   **System (Система)**: Логика, обрабатывающая сущности с определенными компонентами.

ECS в Archetyped основан на доменах. Каждый домен действует как независимый "мир".

## Методы класса ECS

### Инициализация

#### `ECS(std::string name)`
Регистрирует новый домен ECS с заданным именем.
-   **Пример**: `ECS myWorld("GameWorld");`

### Регистрация компонентов

#### `registerComponent<T>(uint32_t hashId, uint32_t capacity)`
Регистрирует тип компонента `T` с конкретным хэш-ID и начальной емкостью.
-   **Пример**: `myWorld.registerComponent<Position>(fnv1aHashConst("Position"), 1000);`

### Управление сущностями (Отложенное)

Эти команды ставятся в очередь и выполняются при вызове `flushCommands()`.

#### `attachComponentDeferred<T>(Entity entity, uint32_t componentHashId, T* componentData)`
Ставит в очередь прикрепление компонента `T` к сущности.
-   **Пример**: `myWorld.attachComponentDeferred(player, posHash, &initialPos);`

#### `removeComponentDeferred<T>(Entity entity, uint32_t componentHashId)`
Ставит в очередь удаление компонента из сущности.
-   **Пример**: `myWorld.removeComponentDeferred<Position>(player, posHash);`

#### `flushCommands()`
Выполняет все поставленные в очередь команды прикрепления/удаления.
-   **Примечание**: Это блокирующая операция (ожидает завершения).

### Запрос данных

#### `getComponent<T>(Entity entity, uint32_t componentHashId)`
Возвращает указатель на данные компонента для данной сущности.
-   **Пример**: `Position* p = myWorld.getComponent<Position>(player, posHash);`

#### `hasComponent(Entity entity, uint32_t componentHashId)`
Проверяет, есть ли у сущности указанный компонент.
-   **Пример**: `bool alive = myWorld.hasComponent(player, healthHash);`

#### `getRawPtr(uint32_t componentId, bool lock = false)`
Возвращает "сырой" указатель на весь массив компонентов для высокоскоростной итерации.
-   **Примечание**: Установка `lock = true` захватит мьютекс для этого массива компонентов.

#### `getGroupSize(std::initializer_list<uint32_t> hashes)`
Возвращает количество сущностей, у которых есть ВСЕ указанные компоненты.

#### `contains(std::initializer_list<uint32_t> array, uint32_t value)`
Утилита для проверки наличия значения в списке хэшей.

### Асинхронные операции

Эти методы возвращают `Ticket*` и не блокируют вызывающий поток.

-   `getComponentAsync(Entity entity, uint32_t componentHashId, void* outputBuffer)`
-   `hasComponentAsync(Entity entity, uint32_t componentHashId, void* outputBuffer)`
-   `getGroupSizeAsync(std::vector<uint32_t> hashes, void* outputBuffer)`
-   `getRawPtrAsync(uint32_t componentId, bool lock, void* outputBuffer)`

### Потокобезопасность

#### `setLock(uint32_t componentId, bool lock)`
Вручную блокирует/разблокирует массив компонентов для безопасного доступа из других потоков.

#### `isLocked(uint32_t componentId)`
Проверяет, заблокирован ли массив компонентов в данный момент.

## Пример использования

```cpp
#include "Engine/ECS.h"

struct Position { float x, y; };
constexpr uint32_t posHash = fnv1aHashConst("Position");

void GameLoop() {
    ECS world("MainWorld");
    world.registerComponent<Position>(posHash, 1000);

    Entity player = { 1 };
    Position startPos = { 0, 0 };

    // Прикрепляем компонент
    world.attachComponentDeferred(player, posHash, &startPos);
    world.flushCommands();

    // Запрашиваем компонент
    Position* p = world.getComponent<Position>(player, posHash);
    if (p) {
        p->x += 1.0f;
    }
}
```
