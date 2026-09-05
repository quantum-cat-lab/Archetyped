# Archetyped: Event Bus (Шина событий)

Сервис Event Bus обеспечивает слабую связанность между различными частями приложения, позволяя им взаимодействовать через события.

## Обзор

Event Bus использует шаблон издатель-подписчик. Вы можете генерировать события немедленно или помещать их в очередь для последующей обработки.

## Методы класса EventBus

### Инициализация

#### `EventBus(std::string name)`
Регистрирует новый домен Event Bus.
-   **Пример**: `EventBus engineEvents("EngineEvents");`

### Подписка

#### `subscribe(uint32_t eventId, EventCallback callback, void* user = nullptr, uint32_t subscriberId = 0)`
Подписывается на событие.
-   `eventId`: Хэш имени события.
-   `callback`: Указатель на функцию типа `void (*)(uint32_t eventId, const EventData& data, void* user)`.
-   `user`: Необязательные пользовательские данные, передаваемые в функцию обратного вызова.
-   `subscriberId`: Необязательный уникальный ID подписчика (используется для отписки).

### Генерация событий

#### `emit(uint32_t eventId, const EventData& data)`
Немедленно вызывает всех подписчиков для данного события. Это блокирующая операция.

#### `push(uint32_t eventId, const EventData& data)`
Помещает событие в очередь. Оно будет обработано при вызове `process()`. Это неблокирующая операция.

#### `process()`
Обрабатывает все события в очереди.

### Управление

#### `unsubscribe(uint32_t subscriberId)`
Удаляет подписчика по его ID.

#### `reset()`
Очищает всех подписчиков и события в домене.

## Пример использования

```cpp
#include "Engine/EventBus.h"

void OnPlayerSpawned(uint32_t id, const EventData& data, void* user) {
    printf("Игрок появился!\n");
}

void Example() {
    EventBus bus("GameBus");
    uint32_t spawnEvent = fnv1aHashConst("PlayerSpawned");

    // Подписка
    bus.subscribe(spawnEvent, OnPlayerSpawned);

    // Немедленная генерация
    EventData d = { nullptr, 0 };
    bus.emit(spawnEvent, d);

    // Помещение в очередь
    bus.push(spawnEvent, d);
    bus.process(); // Теперь вызывается callback для события из очереди
}
```
