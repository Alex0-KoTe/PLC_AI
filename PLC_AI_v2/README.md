# UsKoTE v2 (PLC_AI_v2)

Промышленный 6-канальный температурный контроллер на ESP32. Проект создан на базе структуры PLC_AI v1
(PlatformIO, Arduino-фреймворк) и включает все исправления версии v1: корректный маппинг Modbus,
синхронизация потоков через stateMutex, неблокирующий доступ к архиву, событийный Wi-Fi реконнект.

## Структура проекта
```
├── platformio.ini          # конфигурация сборки (env: uskote_v2)
├── custom_partitions.csv   # таблица разделов flash 4MB (app + littlefs)
├── include/                # заголовки модулей
│   ├── config.h            #   пины, константы регулирования, сеть, EEPROM
│   ├── globals.h           #   общие данные + stateMutex
│   ├── sensors.h control.h ramp.h protection.h   # ядро регулирования
│   ├── archive.h logger.h settings.h             # LittleFS-архив, журнал, EEPROM
│   └── wifi_mgr.h ntp.h webui.h modbus.h         # сеть, время, веб, Modbus TCP
├── src/                    # реализация модулей (13 файлов)
├── lib/                    # пользовательские библиотеки (пусто)
└── test/                   # unit-тесты (pio test)
```

## Сборка и прошивка
```bash
pio run                 # сборка
pio run -t upload       # прошивка
pio device monitor      # лог 115200
```

## Сеть
- Заводской Wi-Fi: SSID `KoTe`, пароль задаётся в `config.h` (DEF_SSID/DEF_PASS)
- Точка доступа для настройки: `UsKoTE-AP` -> веб `/net`
- Modbus TCP: два сервера ( Holding / Input регистры), веб-интерфейс на :80

## Отличия от v1
- Исправлен маппинг sensorMap в записи уставок Modbus
- Все изменения общих данных с Core 0 — под stateMutex
- Архив читается в RAM без удержания fsMutex (потеря записей устранена)
- Антидребезг реле REL_MIN_ON_MS = 1 c
- Реконнект Wi-Fi по событиям (WL_DISCONNECTED) без блокировки loop()
