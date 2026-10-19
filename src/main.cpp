#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "config.h"
#include "globals.h"
#include "sensors.h"
#include "control.h"
#include "protection.h"
#include "ramp.h"
#include "archive.h"
#include "logger.h"
#include "settings.h"
#include "ntp.h"
#include "modbus.h"
#include "webui.h"
#include "wifi_mgr.h"

// Определена в webui.cpp (в webui.h её нет)
void webRegisterRoutes();

// ================ ПИНЫ РЕЛЕ ================
static const uint8_t REL_PIN[6] = {32, 33, 25, 26, 27, 17};

// ================ ЗАДАЧА ВЕБ+MODBUS (Core 0) ================
void webTask(void* pvParameters) {
  for(;;) {
    server.handleClient();   // веб
    mbTask();                // Modbus TCP
    wifiManage();            // WiFi + AP + NTP внутри
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ================ FACTORY RESET ================
static void checkFactory() {
  // [FIX] Примечание: GPIO34 — input-only, внутреннего pull-up нет;
  // подтяжка обеспечивается внешней схемой кнопки. Режим INPUT оставлен,
  // но добавлена защита от ложного сброса: проверка удержания 3 сек уже есть.
  pinMode(FACTORY, INPUT);
  if(digitalRead(FACTORY) == LOW) {
    Serial.println("Кнопка FACTORY нажата, ждём 3 сек...");
    delay(3000);
    if(digitalRead(FACTORY) == LOW) {
      Serial.println("!!! СБРОС НА ЗАВОДСКИЕ !!!");
      defaults();
      eeSave();
      delay(2000);
      ESP.restart();
    }
  }
}

// ================ SETUP ================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== UsKoTE v3: загрузка ===");
  bootReason = (uint8_t)esp_reset_reason();
  
  // --- NVS (для Preferences) ---
  esp_err_t nvsErr = nvs_flash_init();
  Serial.printf("nvs_flash_init: %d\n", nvsErr);
  if(nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  
  // --- EEPROM ---
  EEPROM.begin(256);
  
  // --- Кнопка сброса ---
  checkFactory();
  
  // --- Питание датчиков ---
  pinMode(PULLUP_EN, OUTPUT);
  digitalWrite(PULLUP_EN, HIGH);
  delay(500);
  
  // --- Индикатор ---
  pinMode(ACT_WDT, OUTPUT);
  digitalWrite(ACT_WDT, LOW);
  
  // --- Реле: все закрыты ---
  for(int k = 0; k < 6; k++) {
    pinMode(REL_PIN[k], OUTPUT);
    digitalWrite(REL_PIN[k], LOW);
    rel[k] = false;
    prevRel[k] = false;
  }
  
  // --- ФС ---
  fsOk = LittleFS.begin();
  Serial.printf("LittleFS: %s\n", fsOk ? "OK" : "ОШИБКА");
  // [FIX] Мьютекс общих данных: webTask (Core 0) и loop (Core 1) работают с
  // sp[]/workSp[]/en[]/rel[] и др. одновременно — без защиты возможны
  // рассогласования (state race).
  stateMutex = xSemaphoreCreateMutex();
  if(stateMutex == NULL) {
    Serial.println("!!! ОШИБКА: не удалось создать stateMutex !!!");
    while(1) delay(1000);
  }
  fsMutex = xSemaphoreCreateMutex();
  if(fsMutex == NULL) {
    Serial.println("!!! ОШИБКА: не удалось создать fsMutex !!!");
    while(1) delay(1000);
  }
  Serial.println("fsMutex создан");
  
  // --- Настройки ---
  eeLoad();
  
  // --- Время ---
  loadNtpSettings();
  timeInit();
  
  // --- Датчики и архив ---
  sensorsInit();
  Serial.println(">>> 1. После sensorsInit");
  archInit();
  Serial.println(">>> 2. После archInit");
  
  // [FIX] Построение индекса лога для корректного обновления счётчиков
  logBuildIndex();
  Serial.println(">>> 3. После logBuildIndex");
  
  wifiInit();
  Serial.println(">>> WiFi инициализирован");
  
  // --- Веб + Modbus ---
  webRegisterRoutes();
  Serial.println(">>> 4. После webRegisterRoutes");
  Serial.println(">>> Перед server.begin");
  server.begin();
  Serial.println(">>> 5. После server.begin");
  srvA.begin();
  Serial.println(">>> 6. После srvA.begin");
  srvB.begin();
  Serial.println(">>> 7. После srvB.begin");
  Serial.println(">>> 8. Перед созданием задачи");
  
  // --- Задача Core 0 ---
  xTaskCreatePinnedToCore(webTask, "webTask", 16384, NULL, 1, &webTaskHandle, 0);
  logEv(EV_BOOT, 0, "start");
  Serial.println("=== setup завершён ===");
}

// ================ LOOP (Core 1) ================
static uint32_t lastBlink = 0;
static uint32_t lastSensReq = 0;
static bool sensWaiting = false;
static uint32_t lastMinute = 0;
static uint32_t lastArch = 0;
// [FIX] Для автосохранения EEPROM
static uint32_t lastEeCheck = 0;

void loop() {
  // --- Датчики: асинхронно (запрос -> готово -> чтение) ---
  if(!sensWaiting && millis() - lastSensReq >= 3000) {
    sensorsRequest();
    sensWaiting = true;
    lastSensReq = millis();
  } else if(sensWaiting && sensorsReady()) {
    sensorsRead();
    sensWaiting = false;
  }
  
  // --- Рампы: раз в минуту ---
  if(millis() - lastMinute >= 60000) {
    lastMinute = millis();
    rampTickMinute();
  }
  
  // --- Защиты и регулятор ---
  protectionTick();
  controlTick();
  
  // --- Выключенные каналы: клапан закрыт ---
  for(int k = 0; k < 6; k++) {
    if(!en[k] && rel[k]) rel[k] = false;
  }
  
  // --- Архив: раз в 30 секунд ---
  if(millis() - lastArch >= 30000) {
    lastArch = millis();
    time_t now = time(nullptr);
    if(now > 1600000000) {
      int16_t v[6];
      for(int k = 0; k < 6; k++) {
        v[k] = ok[k] ? (int16_t)roundf(temp[k] * 100.0f) : -32768;
      }
      archWrite((uint32_t)now, v);
    }
  }
  
  // --- Сохранение времени ---
  timeTick();
  
  // --- [FIX] Автосохранение настроек в EEPROM ---
  if(eeDirty && (millis() - lastChangeTime > 3000) && (millis() - lastEeCheck > 5000)) {
    lastEeCheck = millis();
    eeSave();
    eeDirty = false;
    Serial.println(">>> EEPROM сохранён автоматически");
  }
  
  // --- [FIX] Проверка отложенной перезагрузки (из webui) ---
  checkRestart();
  
  // --- Реле на GPIO ---
  // [FIX] Минимальная длительность состояния клапана REL_MIN_ON_MS:
  // без неё при температуре ровно на пороге гистерезиса реле могло
  // щёлкать каждые несколько секунд (износ клапана/реле).
  static uint32_t lastRelSwitch[6] = {0};
  for(int k = 0; k < 6; k++) {
    if(rel[k] != prevRel[k]) {
      if(millis() - lastRelSwitch[k] >= REL_MIN_ON_MS || !prevRel[k] && lastRelSwitch[k] == 0) {
        digitalWrite(REL_PIN[k], rel[k] ? HIGH : LOW);
        prevRel[k] = rel[k];
        lastRelSwitch[k] = millis();
      } else {
        rel[k] = prevRel[k];   // откатываем желание переключиться раньше времени
      }
    }
  }
  
  // --- Индикатор + blinkBit для Modbus ---
  // [FIX] Теперь blinkBit переключается синхронно со светодиодом
  if(millis() - lastBlink >= 500) {
    lastBlink = millis();
    digitalWrite(ACT_WDT, !digitalRead(ACT_WDT));
    blinkBit = !blinkBit;   // ← ДОБАВЛЕНО: переключение бита для Modbus
  }
}