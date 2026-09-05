# Archetyped: Планирование и выполнение систем

Эти сервисы предоставляют механизмы для запуска периодической логики, отложенных задач и высокоуровневых систем (подобных ECS).

## Smart Scheduler

Smart Scheduler управляет задачами по времени и "тиками" (ticks).

### Методы FUR

-   `scheduleCyclicHash`: Запускает задачу каждые `N` миллисекунд.
-   `scheduleDelayedHash`: Запускает задачу один раз после задержки.
-   `scheduleTickHash`: Запускает задачу каждый тик движка.

### Пример (Использование FUR напрямую)

```cpp
#include "Engine/FractalSDK.h"

void MyPeriodicTask(void* ctx) {
    // Логика...
}

void Setup() {
    scheduleSSCMDContext ctx;
    ctx.fn = MyPeriodicTask;
    ctx.context = nullptr;
    ctx.value = 1000; // Интервал 1000 мс

    FURCMDPacket packet;
    packet.methodHash = scheduleCyclicHash;
    packet.payloadSize = sizeof(ctx);
    packet.payload = &ctx;

    FractalSDK::SDK::Get()->sendPacket(packet);
}
```

## System Execution

Сервис System Execution — это высокоуровневая обертка, использующая Smart Scheduler для запуска систем игровой логики.

### Методы FUR

-   `registerSystemHash`: Регистрирует функцию для вызова с определенной частотой.

### Пример

```cpp
void MySystem(void* ctx) {
    // Логика обновления...
}

void Register() {
    registerSystemCMDContext ctx;
    ctx.fn = MySystem;
    ctx.context = nullptr;
    ctx.frequencyMs = 0; // 0 для каждого тика

    FURCMDPacket packet;
    packet.methodHash = registerSystemHash;
    packet.payloadSize = sizeof(ctx);
    packet.payload = &ctx;

    FractalSDK::SDK::Get()->sendPacket(packet);
}
```
