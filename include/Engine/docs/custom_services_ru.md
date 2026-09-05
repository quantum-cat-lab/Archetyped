# Archetyped: Создание пользовательских сервисов

Архитектура Archetyped позволяет легко добавлять собственные сервисы, регистрируя методы FUR в ядре.

## Шаг 1: Определение логики сервиса

Создайте класс или набор функций, которые будут обрабатывать команды.

```cpp
// MyService.h
void MyCustomMethod(FURCMDPacket& packet) {
    // 1. Получаем входные данные из packet.payload
    // 2. Выполняем логику
    // 3. (Опционально) Записываем результат в packet.outputBuffer
    // 4. Устанавливаем packet.fence в 1, если он присутствует
    if (packet.fence) {
        *packet.fence = 1;
    }
}
```

## Шаг 2: Регистрация сервиса в ядре

Вам нужно зарегистрировать свои методы с уникальным 32-битным хэшем.

```cpp
uint32_t myMethodHash = fnv1aHashConst("my_engine:my_service:my_method");
kernel->registerCMDMethod(myMethodHash, MyCustomMethod);
```

## Шаг 3: Создание обертки в SDK (Опционально, но рекомендуется)

Для удобства использования создайте C++ класс, который инкапсулирует создание пакетов FUR.

```cpp
class MyService {
public:
    void doSomething(int value) {
        Ticket* ticket = SDK::Get()->allocateTicket();
        
        FURCMDPacket packet;
        packet.methodHash = myMethodHash;
        packet.payload = &value;
        packet.payloadSize = sizeof(int);
        packet.fence = (uint64_t*)&ticket->fence;

        SDK::Get()->sendPacket(packet);
        
        // Ожидание завершения
        while(!ticket->isReady()) {}
    }
};
```

## Рекомендации

1.  **Структуры контекста**: Используйте структуры для полезной нагрузки (payload), если нужно передать несколько аргументов.
2.  **Потокобезопасность**: Помните, что методы FUR выполняются в рабочем потоке. Убедитесь, что логика вашего сервиса потокобезопасна.
3.  **Именование**: Используйте четкое пространство имен для хэшей ваших методов, чтобы избежать коллизий (например, `автор:сервис:метод`).
