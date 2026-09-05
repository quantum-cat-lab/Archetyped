# Archetyped: Work Scheduler (Планировщик работ)

Сервис Work Scheduler управляет пулом потоков для выполнения фоновых задач.

## Обзор

Вы можете перенести тяжелые вычисления в Work Scheduler, чтобы основной поток оставался отзывчивым.

## Методы класса WorkScheduler

### Инициализация

#### `WorkScheduler(std::string name)`
Регистрирует новый домен Work Scheduler.

### Планирование задач

#### `schedule(w_task_fn fn, void* context)`
Планирует задачу для выполнения в пуле потоков.
-   `fn`: Указатель на функцию типа `void (*)(void* context)`.
-   `context`: Указатель на данные, необходимые функции.

## Пример использования

```cpp
#include "Engine/WorkScheduler.h"

struct MyTaskData {
    int result;
};

void ComplexCalculation(void* ctx) {
    MyTaskData* data = static_cast<MyTaskData*>(ctx);
    // Выполняем тяжелую работу...
    data->result = 42;
}

void Example() {
    WorkScheduler scheduler("HeavyTasks");
    MyTaskData data;

    scheduler.schedule(ComplexCalculation, &data);

    // Задача выполняется в фоновом режиме.
    // Используйте Билеты (Tickets) или другие механизмы синхронизации, если вам нужно дождаться завершения.
}
```
