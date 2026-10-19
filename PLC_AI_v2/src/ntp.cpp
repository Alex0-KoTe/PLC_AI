#include "ntp.h"
#include "globals.h"
#include "logger.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <sys/time.h>
#include <time.h>

static uint32_t lastTimeSave = 0;

// ================================================================
//  СОБСТВЕННАЯ РЕАЛИЗАЦИЯ timegm (нет в newlib ESP32)
//  Преобразует struct tm (трактуется как UTC) в time_t.
//  Алгоритм Говарда Хиннанта (civil days). ghghghghghghghghghggh       test Git
// ================================================================
static time_t myTimegm(struct tm* t) {
  int m = t->tm_mon + 1;          // 1-12
  int y = t->tm_year + 1900;
  
  if(m <= 2) y -= 1;              // Янв и Фев относим к прошлому году
  
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + t->tm_mday - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + doe - 719468;
  
  return days * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
}

// Публичная обёртка (если объявлена в ntp.h)
time_t makeUtcTimestamp(int year, int month, int day, int hour, int minute, int second) {
  struct tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min  = minute;
  t.tm_sec  = second;
  return myTimegm(&t);
}

// ================================================================
//  ЧАСОВОЙ ПОЯС
// ================================================================
void applyTimezone() {
  char tzString[32];
  // POSIX-формат: знак инвертируется! UTC+7 -> "UTC-7"
  snprintf(tzString, sizeof(tzString), "UTC%+d", -ntpOffsetHours);
  setenv("TZ", tzString, 1);
  tzset();
  Serial.printf("NTP: часовой пояс применён: %s (UTC%+d)\n", tzString, ntpOffsetHours);
}

// ================================================================
//  ВРЕМЯ ПО УМОЛЧАНИЮ (26.08.2026 09:00 местного)
// ================================================================
time_t getDefaultTime() {
  struct tm t = {0};
  t.tm_year = DEFAULT_TIME_YEAR - 1900;
  t.tm_mon  = DEFAULT_TIME_MONTH - 1;
  t.tm_mday = DEFAULT_TIME_DAY;
  t.tm_hour = DEFAULT_TIME_HOUR;
  t.tm_min  = DEFAULT_TIME_MIN;
  t.tm_sec  = 0;
  
  // myTimegm трактует как UTC; вычитаем пояс -> UTC-метка местного времени
  return myTimegm(&t) - (time_t)ntpOffsetHours * 3600;
}

// ================================================================
//  ИНИЦИАЛИЗАЦИЯ ВРЕМЕНИ ПРИ СТАРТЕ
// ================================================================
void timeInit() {
  Serial.println("Время: инициализация...");
  
  preferences.begin("time", true);
  long savedTs = preferences.getLong("ts", 0);
  preferences.end();
  
  time_t startTime;
  
  if(savedTs > 1577836800) {
    startTime = (time_t)savedTs;
    Serial.printf("Время: восстановлено из памяти (ts=%ld)\n", startTime);
  } else {
    startTime = getDefaultTime();
    Serial.println("Время: установлено по умолчанию 26.08.2026 09:00");
  }
  
  struct timeval tv;
  tv.tv_sec = startTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  
  applyTimezone();
  
  ntpSynced = false;
  Serial.printf("Время: локально сейчас %s\n", getFormattedTime().c_str());
}

// ================================================================
//  РУЧНАЯ УСТАНОВКА ВРЕМЕНИ
// ================================================================
bool setManualTime(int year, int month, int day, int hour, int min, int sec) {
  if(year < 2020 || year > 2100) return false;
  if(month < 1 || month > 12) return false;
  if(day < 1 || day > 31) return false;
  if(hour < 0 || hour > 23) return false;
  if(min < 0 || min > 59) return false;
  if(sec < 0 || sec > 59) return false;
  
  struct tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min  = min;
  t.tm_sec  = sec;
  
  // Локальное время -> UTC-метка
  time_t ts = myTimegm(&t) - (time_t)ntpOffsetHours * 3600;
  
  struct timeval tv;
  tv.tv_sec = ts;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  
  applyTimezone();
  
  ntpSynced = false;   // время установлено вручную
  saveCurrentTime();   // сразу сохраняем
  
  Serial.printf("Время: установлено вручную %02d.%02d.%04d %02d:%02d:%02d\n",
                day, month, year, hour, min, sec);
  return true;
}

// ================================================================
//  СОХРАНЕНИЕ ВРЕМЕНИ
// ================================================================
void saveCurrentTime() {
  time_t now = time(nullptr);   // UTC-метка
  
  if(now > 1577836800) {
    preferences.begin("time", false);
    preferences.putLong("ts", (long)now);
    preferences.end();
  }
}

void timeTick() {
  if(millis() - lastTimeSave >= TIME_SAVE_INTERVAL) {
    lastTimeSave = millis();
    saveCurrentTime();
    Serial.printf("Время: сохранено (%s)\n", getFormattedTime().c_str());
  }
}

// ================================================================
//  ПИНГ NTP-СЕРВЕРА (ДИАГНОСТИКА)
// ================================================================
static bool ntpPingServer(const char* host, uint32_t timeoutMs = 2000) {
  IPAddress ip;
  
  // --- DNS ---
  uint32_t t = millis();
  if(WiFi.hostByName(host, ip) != 1) {
    Serial.printf("[NTP-PING] %s: ОШИБКА DNS (имя не разрешено)\n", host);
    return false;
  }
  Serial.printf("[NTP-PING] %s -> %s (DNS %u мс)\n",
                host, ip.toString().c_str(), (unsigned)(millis() - t));
  
  // --- UDP запрос ---
  WiFiUDP udp;
  udp.begin(23456);
  
  uint8_t pkt[48];
  memset(pkt, 0, sizeof(pkt));
  pkt[0] = 0x1B;   // NTP-клиент, версия 3
  
  t = millis();
  if(!udp.beginPacket(ip, 123)) {
    Serial.printf("[NTP-PING] %s: ОШИБКА отправки\n", host);
    udp.stop();
    return false;
  }
  udp.write(pkt, sizeof(pkt));
  udp.endPacket();
  
  // --- Ждём ответ ---
  while(millis() - t < timeoutMs) {
    int n = udp.parsePacket();
    if(n >= 48) {
      uint8_t buf[48];
      udp.read(buf, 48);
      
      // Время передачи сервера (байты 40-43), секунд с 1900
      uint32_t sec = ((uint32_t)buf[40] << 24) | ((uint32_t)buf[41] << 16) |
                     ((uint32_t)buf[42] << 8)  | (uint32_t)buf[43];
      uint32_t unixSec = sec - 2208988800UL;   // -> unix-время
      
      Serial.printf("[NTP-PING] %s: ОТВЕТ, RTT=%u мс, время сервера=%lu\n",
                    host, (unsigned)(millis() - t), (unsigned long)unixSec);
      udp.stop();
      return true;
    }
    delay(10);
  }
  
  Serial.printf("[NTP-PING] %s: НЕТ ответа за %u мс\n", host, timeoutMs);
  udp.stop();
  return false;
}

// ================================================================
//  NTP СИНХРОНИЗАЦИЯ
// ================================================================
bool initNTP() {
  Serial.println("NTP: Запуск синхронизации...");
  
  // --- Диагностика доступности серверов ---
  Serial.println("NTP: проверка доступности серверов...");
  ntpPingServer(NTP_SERVER1,2000);
  ntpPingServer(NTP_SERVER2,2000);
  ntpPingServer(NTP_SERVER3,2000);
  
  // Запоминаем текущее время, чтобы восстановить при неудаче
  time_t savedNow = time(nullptr);
  
  // Сбрасываем системное время в 1970 —
  // теперь getLocalTime вернёт true ТОЛЬКО после реальной синхронизации
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  
  // Запрашиваем чистый UTC (смещение 0); пояс применим один раз сами
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  
  struct tm timeinfo;
  uint32_t start = millis();
  bool synced = false;
  
  while(millis() - start < NTP_TIMEOUT_MS) {
    if(getLocalTime(&timeinfo, 0) && (timeinfo.tm_year + 1900) >= 2020) {
      synced = true;
      break;
    }
    delay(100);
    yield();
  }
  
  if(!synced) {
    Serial.println("NTP: Таймаут — восстанавливаем прежнее время");
    tv.tv_sec = savedNow;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    applyTimezone();
    return false;
  }
  
  // Применяем часовой пояс (один раз)
  applyTimezone();
  
  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);
  Serial.print("NTP: Синхронизировано: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  
  ntpSynced = true;
  lastNtpSync = millis();
  saveCurrentTime();   // сохраняем корректный UTC
  
  return true;
}

// ================================================================
//  УПРАВЛЕНИЕ NTP (вызывается периодически)
// ================================================================
void manageNTP() {
  // Если галочка NTP выключена — не синхронизируем вообще
  if(!ntpEnabled) return;
  
  // Не синхронизировано + есть WiFi — попытка раз в 5 минут
  if(!ntpSynced && WiFi.status() == WL_CONNECTED) {
    if(millis() - ntpAttemptTime > 300000UL) {
      ntpAttemptTime = millis();
      if(initNTP()) {
        logEv(EV_WIFI_ON, 0, "NTP OK");
      }
    }
    return;
  }
  
  // Плановая ресинхронизация раз в сутки
  if(ntpSynced && (millis() - lastNtpSync > NTP_SYNC_INTERVAL)) {
    Serial.println("NTP: Плановая ресинхронизация");
    if(initNTP()) {
      lastNtpSync = millis();
    } else {
      lastNtpSync = millis() - NTP_SYNC_INTERVAL + 3600000UL;
    }
  }
  
  // Проверка валидности
  time_t now = time(nullptr);
  if(now < 1577836800 && ntpSynced) {
    ntpSynced = false;
  }
}

// ================================================================
//  ФОРМАТИРОВАНИЕ ВРЕМЕНИ
// ================================================================
String getFormattedTime() {
  time_t now = time(nullptr);
  
  if(now < 1577836800) return "Время не установлено";
  
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  
  String result = String(buffer);
  
  if(ntpSynced) {
    result += " (NTP)";
  } else if(ntpEnabled) {
    result += " (ожидание NTP)";
  } else {
    result += " (вручную)";
  }
  
  return result;
}

String getShortTime() {
  time_t now = time(nullptr);
  
  if(now < 1577836800) return "--:--";
  
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

// ================================================================
//  НАСТРОЙКИ
// ================================================================
void saveNtpSettings() {
  preferences.begin("ntp", false);
  preferences.putInt("offset", ntpOffsetHours);
  preferences.putBool("usentp", ntpEnabled);
  preferences.end();
  Serial.printf("NTP: сохранено (пояс UTC%+d, NTP %s)\n",
                ntpOffsetHours, ntpEnabled ? "вкл" : "выкл");
}

void loadNtpSettings() {
  preferences.begin("ntp", true);
  ntpOffsetHours = preferences.getInt("offset", NTP_DEFAULT_OFFSET);
  ntpEnabled = preferences.getBool("usentp", true);
  preferences.end();
  Serial.printf("NTP: загружено (пояс UTC%+d, NTP %s)\n",
                ntpOffsetHours, ntpEnabled ? "вкл" : "выкл");
}