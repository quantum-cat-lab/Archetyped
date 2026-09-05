# Archetyped: Загрузчик модулей

Сервис Module Loader позволяет движку загружать динамические библиотеки (DLL/Shared Objects) во время выполнения.

## Обзор

Модули — это внешние библиотеки, которые могут регистрировать новые сервисы или расширять существующие. Каждый модуль должен иметь точку входа, принимающую `KernelAPI`.

## Точка входа модуля

Модуль должен экспортировать функцию, совместимую с `ModuleEntry`:

```cpp
extern "C" void FractalModuleEntry(KernelAPI api) {
    // Регистрируйте пользовательские методы FUR здесь
    // api.registerCMDMethod(...);
}
```

## Загрузка модулей через SDK

### Методы FUR

-   `loadModuleHash`: Загружает модуль по указанному пути к файлу.
-   `isLoadedHash`: Проверяет, загружен ли модуль уже.

### Пример

```cpp
loadModuleContext ctx;
ctx.path = "path/to/my_module.so";

FURCMDPacket packet;
packet.methodHash = loadModuleHash;
packet.payloadSize = sizeof(ctx);
packet.payload = &ctx;

FractalSDK::SDK::Get()->sendPacket(packet);
```
