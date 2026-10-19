#include "logger.h"
#include "globals.h"
#include "ntp.h"
#include <LittleFS.h>

struct LogIdx {
  uint8_t code, ch;
  uint32_t pos, cnt;
  String val;
};

static LogIdx lidx[LOG_IDX_N];
static int lidxCount = 0;
static uint32_t logSize = 0;

// [FIX] Унифицированы коды событий — используются EV_ERR1/2/3 из logger.h
String evName(uint8_t c) {
  switch(c) {
    case EV_WIFI_ON:  return "WiFi подключение";
    case EV_WIFI_OFF: return "WiFi потеря";
    case EV_ERR1:     return "АВАРИЯ: нет охлаждения";
    case EV_ERR2:     return "АВАРИЯ: неконтр. нагрев";
    case EV_ERR3:     return "АВАРИЯ: неконтр. охлаждение";
    case EV_ERRCLR:   return "Авария сброшена";
    case EV_SP:       return "Изменение уставки";
    case EV_CH_ON:    return "Включение канала";
    case EV_CH_OFF:   return "Выключение канала";
    case EV_RAMP_ON:  return "Старт рампа";
    case EV_RAMP_HR:  return "Час рампа пройден";
    case EV_RAMP_RS:  return "Восстановление рампа";
    case EV_WEB:      return "Заход в веб";
    case EV_BOOT:     return "Старт устройства";
  }
  return "Событие " + String(c);
}

static int logFind(uint8_t code, uint8_t ch, const String& val) {
  for(int i = 0; i < lidxCount; i++)
    if(lidx[i].code == code && lidx[i].ch == ch && lidx[i].val == val)
      return i;
  return -1;
}

static String logLine(uint8_t code, uint32_t cnt, uint8_t ch, const String& val,
                      unsigned long up, const char* ts) {
  char v[17]; strncpy(v, val.c_str(), 16); v[16] = 0;
  char d[20]; strncpy(d, ts, 19); d[19] = 0;
  char b[LOG_LINE + 1];
  snprintf(b, sizeof(b), "%02u;%06u;%u;%-16s;%08lu;%-19s\n", code, cnt, ch, v, up, d);
  return String(b);
}

void logBuildIndex() {
  if(!fsOk || !fsMutex) return;
  if(xSemaphoreTake(fsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    lidxCount = 0;
    logSize = 0;
    File f = LittleFS.open("/log.csv", "r");
    if(!f) {
      xSemaphoreGive(fsMutex);
      return;
    }
    logSize = f.size();
    String first = f.readStringUntil('\n');
    if(first.length() > 0 && !first.startsWith("code;") && first.length() != LOG_LINE - 1) {
      f.close();
      LittleFS.remove("/log.old");
      LittleFS.rename("/log.csv", "/log.old");
      logSize = 0;
      xSemaphoreGive(fsMutex);
      return;
    }
    f.seek(0);
    uint32_t pos = 0;
    while(f.available()) {
      String l = f.readStringUntil('\n');
      uint16_t ll = l.length() + 1;
      if(l.length() == LOG_LINE - 1 && !l.startsWith("code;")) {
        int a = l.indexOf(';');
        int b = l.indexOf(';', a + 1);
        int c = l.indexOf(';', b + 1);
        int d = l.indexOf(';', c + 1);
        if(a > 0 && b > 0 && c > 0 && d > 0) {
          uint8_t code = l.substring(0, a).toInt();
          uint8_t ch = l.substring(b + 1, c).toInt();
          String val = l.substring(c + 1, d);
          val.trim();
          if(code == EV_RAMP_ON || code == EV_RAMP_HR) val = "";
          int i = logFind(code, ch, val);
          if(i < 0 && lidxCount < LOG_IDX_N) i = lidxCount++;
          if(i >= 0) {
            lidx[i].code = code;
            lidx[i].ch = ch;
            lidx[i].val = val;
            lidx[i].pos = pos;
            lidx[i].cnt = l.substring(a + 1, b).toInt();
          }
        }
      }
      pos += ll;
    }
    f.close();
    xSemaphoreGive(fsMutex);
  }
}

void logEv(uint8_t code, uint8_t ch, const String& val) {
  if(!fsOk || !fsMutex) return;
  if(xSemaphoreTake(fsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    char tbuf[40];
    const char* tstr;
    if(!ntpSynced) {
      tstr = "ntp-no";
    } else {
      time_t now = time(nullptr);
      struct tm tm;
      localtime_r(&now, &tm);
      if(tm.tm_year + 1900 < 2020) {
        tstr = "ntp-invalid";
      } else {
        snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
        tstr = tbuf;
      }
    }
    unsigned long up = millis() / 1000;
    String kv = val;
    if(code == EV_RAMP_ON || code == EV_RAMP_HR) kv = "";
    int i = logFind(code, ch, kv);
    if(i >= 0) {
      File w = LittleFS.open("/log.csv", "r+");
      if(w) {
        w.seek(lidx[i].pos);
        w.print(logLine(code, lidx[i].cnt + 1, ch, val, up, tstr));
        w.close();
        lidx[i].cnt++;
      }
      xSemaphoreGive(fsMutex);
      return;
    }
    if(logSize > LOG_MAX) {
      LittleFS.remove("/log.old");
      LittleFS.rename("/log.csv", "/log.old");
      lidxCount = 0;
      logSize = 0;
    }
    File f = LittleFS.open("/log.csv", FILE_APPEND);
    if(f) {
      if(f.size() == 0) f.println("code;counter;ch;value;uptime;datetime");
      uint32_t p = f.size();
      f.print(logLine(code, 1, ch, val, up, tstr));
      logSize = f.size();
      if(lidxCount < LOG_IDX_N) {
        lidx[lidxCount].code = code;
        lidx[lidxCount].ch = ch;
        lidx[lidxCount].val = kv;
        lidx[lidxCount].pos = p;
        lidx[lidxCount].cnt = 1;
        lidxCount++;
      }
      f.close();
    }
    xSemaphoreGive(fsMutex);
  } else {
    // [FIX] Логируем потерю события
    Serial.println("[LOG] Не удалось захватить мьютекс, событие потеряно");
  }
}

// Экспорт для webui
void logGetStats(uint32_t& size, int& count) {
  size = logSize;
  count = lidxCount;
}