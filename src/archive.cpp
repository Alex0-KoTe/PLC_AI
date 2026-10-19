#include "archive.h"
#include "globals.h"
#include "logger.h"
#include <LittleFS.h>
#include <time.h>

// ================ КОНСТАНТЫ ================
#define ARCH_REC_SIZE       16        // 4 байта времени + 6 × int16 = 16 байт
#define ARCH_RECS_PER_DAY   2880      // 24ч × 60мин / 0.5мин = 2880 записей
//===#define ARCH_DAY_FILE_SIZE  (ARCH_RECS_PER_DAY * ARCH_REC_SIZE)   // 46080 байт
#define ARCH_MIN_FREE_SPACE 100000    // минимум 100 КБ свободно
#define ARCH_MAX_DAYS       60        // максимум 60 файлов (подстраховка)

// ================ СТАТИЧЕСКИЕ БУФЕРЫ (не на стеке) ================
static uint32_t sDays[ARCH_MAX_DAYS];   // общий буфер для списка дат

// ================ ВСПОМОГАТЕЛЬНЫЕ ================
uint32_t archDateFromTime(uint32_t timestamp) {
  time_t t = (time_t)timestamp;
  struct tm tm;
  localtime_r(&t, &tm);
  return (uint32_t)((tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday);
}

static uint32_t secondsFromMidnight(uint32_t timestamp) {
  time_t t = (time_t)timestamp;
  struct tm tm;
  localtime_r(&t, &tm);
  return (uint32_t)(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
}

size_t archGetFreeSpace() {
  if(!fsOk) return 0;
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

bool archDayExists(uint32_t dateNum) {
  if(!fsOk) return false;
  char fname[32];
  snprintf(fname, sizeof(fname), "/arch_%08lu.bin", dateNum);
  return LittleFS.exists(fname);
}

// ================ СПИСОК ДНЕЙ (оптимизирован, без String) ================
int archGetDayList(uint32_t* days, int maxCount) {
  if(!fsOk) return 0;
  if(maxCount > ARCH_MAX_DAYS) maxCount = ARCH_MAX_DAYS;
  
  int count = 0;
  File root = LittleFS.open("/");
  if(!root || !root.isDirectory()) return 0;
  
  File file = root.openNextFile();
  while(file && count < maxCount) {
    const char* name = file.name();
    if(name && *name) {
      if(*name == '/') name++;
      
      // Проверка: "arch_" + 8 цифр + ".bin"
      if(strncmp(name, "arch_", 5) == 0) {
        const char* p = name + 5;
        bool allDigits = true;
        for(int i = 0; i < 8; i++) {
          if(p[i] < '0' || p[i] > '9') { allDigits = false; break; }
        }
        
        if(allDigits && strncmp(p + 8, ".bin", 4) == 0 && p[12] == '\0') {
          uint32_t dateNum = 0;
          for(int i = 0; i < 8; i++) {
            dateNum = dateNum * 10 + (p[i] - '0');
          }
          if(dateNum >= 20200101 && dateNum <= 21001231) {
            days[count++] = dateNum;
          }
        }
      }
    }
    file = root.openNextFile();
  }
  
  // Сортировка по возрастанию
  for(int i = 0; i < count - 1; i++) {
    for(int j = i + 1; j < count; j++) {
      if(days[j] < days[i]) {
        uint32_t tmp = days[i];
        days[i] = days[j];
        days[j] = tmp;
      }
    }
  }
  
  return count;
}

// ================ ОЧИСТКА (без мьютекса, static буфер) ================
void archCleanup() {
  if(!fsOk) return;
  
  size_t freeSpace = archGetFreeSpace();
  Serial.printf("[АРХИВ] Проверка места: свободно %u байт\n", freeSpace);
  
  while(freeSpace < ARCH_MIN_FREE_SPACE) {
    int count = archGetDayList(sDays, ARCH_MAX_DAYS);
    
    if(count <= 1) {
      Serial.println("[АРХИВ] Нечего удалять, но места мало");
      break;
    }
    
    char fname[32];
    snprintf(fname, sizeof(fname), "/arch_%08lu.bin", sDays[0]);
    Serial.printf("[АРХИВ] Удаляем старый файл %s\n", fname);
    
    LittleFS.remove(fname);
    freeSpace = archGetFreeSpace();
  }
}

// ================ ЧТЕНИЕ ДНЯ (с мьютексом, защитами) ================
int archReadDay(uint32_t dateNum, uint8_t* data) {
  if(!fsOk || !data) return 0;
  
  if(fsMutex == NULL) {
    Serial.println("[АРХИВ] fsMutex не создан");
    return 0;
  }
  
  char fname[32];
  snprintf(fname, sizeof(fname), "/arch_%08lu.bin", dateNum);
  
  if(xSemaphoreTake(fsMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    Serial.println("[АРХИВ] Не удалось захватить мьютекс для чтения");
    return 0;
  }
  
  File f = LittleFS.open(fname, "r");
  int bytesRead = 0;
  
  if(f && f.size() == ARCH_DAY_FILE_SIZE) {
    bytesRead = f.read(data, ARCH_DAY_FILE_SIZE);
  }
  
  if(f) f.close();
  xSemaphoreGive(fsMutex);
  
  return bytesRead;
}

// ================ ЗАПИСЬ (Core 1, с мьютексом) ================
void archWrite(uint32_t t, const int16_t* v) {
  if(!fsOk || !v) return;
  
  if(fsMutex == NULL) return;
  
  uint32_t dateNum = archDateFromTime(t);
  uint32_t sec = secondsFromMidnight(t);
  uint32_t recIdx = sec / 30;
  if(recIdx >= ARCH_RECS_PER_DAY) return;
  
  char fname[32];
  snprintf(fname, sizeof(fname), "/arch_%08lu.bin", dateNum);
  
  if(xSemaphoreTake(fsMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    Serial.println("[АРХИВ] Не удалось захватить мьютекс для записи");
    return;
  }
  
  // Создаём файл, если его нет
  if(!LittleFS.exists(fname)) {
    // Проверка места перед созданием
    if(archGetFreeSpace() < ARCH_DAY_FILE_SIZE + 20000) {
      xSemaphoreGive(fsMutex);
      Serial.println("[АРХИВ] Недостаточно места для нового файла");
      return;
    }
    
    File f = LittleFS.open(fname, "w");
    if(!f) {
      xSemaphoreGive(fsMutex);
      Serial.println("[АРХИВ] Ошибка создания файла");
      return;
    }
    
    // Забиваем файл "нет данных"
    uint8_t blank[ARCH_REC_SIZE];
    blank[0] = blank[1] = blank[2] = blank[3] = 0;
    for(int k = 0; k < 6; k++) {
      blank[4 + 2*k]     = 0x00;
      blank[5 + 2*k]     = 0x80;  // -32768 (little-endian)
    }
    
    for(int i = 0; i < ARCH_RECS_PER_DAY; i++) {
      f.write(blank, ARCH_REC_SIZE);
    }
    f.close();
    
    Serial.printf("[АРХИВ] Создан новый файл %s\n", fname);
  }
  
  // Дописываем запись
  File f = LittleFS.open(fname, "r+");
  if(!f || f.size() != ARCH_DAY_FILE_SIZE) {
    if(f) f.close();
    xSemaphoreGive(fsMutex);
    return;
  }
  
  uint32_t pos = recIdx * ARCH_REC_SIZE;
  if(!f.seek(pos)) {
    f.close();
    xSemaphoreGive(fsMutex);
    return;
  }
  
  uint8_t rec[ARCH_REC_SIZE];
  rec[0] = (uint8_t)(t);
  rec[1] = (uint8_t)(t >> 8);
  rec[2] = (uint8_t)(t >> 16);
  rec[3] = (uint8_t)(t >> 24);
  
  for(int k = 0; k < 6; k++) {
    int16_t val = v[k];
    rec[4 + 2*k]     = (uint8_t)(val);
    rec[5 + 2*k]     = (uint8_t)(val >> 8);
  }
  
  f.write(rec, ARCH_REC_SIZE);
  f.close();
  
  xSemaphoreGive(fsMutex);
}

// ================ ИНИЦИАЛИЗАЦИЯ (при старте, без мьютекса) ================
void archInit() {
  Serial.println("[АРХИВ] Инициализация");
  
  if(!fsOk) {
    Serial.println("[АРХИВ] ФС не готова");
    return;
  }
  
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  Serial.printf("[АРХИВ] LittleFS: total=%u, used=%u, free=%u\n", 
                total, used, total - used);
  
  // Очистка при нехватке места
  archCleanup();
  
  // Список дней (используем статический буфер)
  int count = archGetDayList(sDays, ARCH_MAX_DAYS);
  Serial.printf("[АРХИВ] Найдено дней: %d\n", count);
  
  if(count > 0) {
    Serial.printf("[АРХИВ] Последний день: %08lu\n", sDays[count-1]);
  }
  
  Serial.println("[АРХИВ] Инициализация завершена");
}