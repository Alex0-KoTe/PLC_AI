#pragma once
//================ ПИНЫ ================
#define PULLUP_EN 4
#define ACT_WDT   16
#define FACTORY   34
//================ КОНСТАНТЫ РЕГУЛИРОВАНИЯ ================
#define HYST      0.1f
#define REL_MIN_ON_MS 60000UL  // [FIX] мин. время между переключениями одного клапана (антидребезг)
#define DEF_SP    10.0f
#define DEF_DELTA 0.5f
#define HOUR_MS   3600000UL
#define NO_COOL_TIME 1200000UL
#define HEAT_DELAY_MS  900000UL   // 15 мин: задержка активации защиты "нагрев" после включения клапана
#define ERR_AUTO_RESET_MS  3600000UL   // 1 час: автосброс ошибок
//================ СЕТЬ ================
// [FIX] Убран зашитый чужой WiFi-пароль: устройство подключается только
// к сети, настроенной пользователем (веб/AP UsKoTE-AP -> /net).
#define DEF_SSID  ""
#define DEF_PASS  ""
#define AP_SSID   "UsKoTE-AP"
#define AP_PASS   "12345678"
//================ MODBUS ================
#define HREG_N    43
#define IN_N      12
//================ КОДЫ СОБЫТИЙ ================
// [FIX] Удалены дубли EV_ERR_NC/HT/CL — используются EV_ERR1/2/3 из logger.h
#define EV_WIFI_ON  1
#define EV_WIFI_OFF 2
#define EV_SP       6
#define EV_CH_ON    7
#define EV_RAMP_ON  8
#define EV_RAMP_HR  9
#define EV_RAMP_RS  10
#define EV_WEB      11
#define EV_BOOT     12
#define EV_CH_OFF   13
//================ EEPROM ================
#define EE_MAGIC  0xAA
#define EE_SIZE   156
//================ АРХИВ (НОВАЯ АРХИТЕКТУРА: файл на день) ================
#define ARCH_INTERVAL_SEC    30          // Интервал записи, секунд
#define ARCH_REC_PER_DAY     2880        // Записей в сутки (86400/30)
#define ARCH_REC_SIZE        16          // Размер одной записи, байт
#define ARCH_DAY_FILE_SIZE   (ARCH_REC_PER_DAY * ARCH_REC_SIZE)  // 46080 байт
#define ARCH_MIN_FREE_SPACE  100000     // Минимум свободного места (100 КБ)
#define ARCH_MAX_DAYS        60          // Максимум дней хранения (защита)
//================ ЛОГ ================
#define LOG_MAX    300000UL
#define LOG_LINE   58
#define LOG_IDX_N  64
#define TAIL_N     100
//================ NTP ================
#define NTP_SERVER1        "194.190.168.1"
#define NTP_SERVER2        "194.0.116.4"
#define NTP_SERVER3        "85.21.228.78"
#define NTP_SYNC_INTERVAL  86400000UL      // Ресинхронизация раз в сутки (мс)
#define NTP_TIMEOUT_MS     15000           // Таймаут ожидания синхронизации
#define NTP_RETRY_MS       30000           // Повторная попытка при ошибке
#define NTP_DEFAULT_OFFSET 3               // Часовой пояс по умолчанию (МСК)
//================ ВРЕМЯ ПО УМОЛЧАНИЮ ================
#define DEFAULT_TIME_YEAR    2026
#define DEFAULT_TIME_MONTH   8      // Август (1-12)
#define DEFAULT_TIME_DAY     26
#define DEFAULT_TIME_HOUR    9      // 09:00
#define DEFAULT_TIME_MIN     0
#define TIME_SAVE_INTERVAL   3600000UL  // Сохранение времени каждый час (мс)