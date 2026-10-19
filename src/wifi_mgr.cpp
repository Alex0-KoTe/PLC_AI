#include "wifi_mgr.h"
#include "globals.h"
#include "logger.h"
#include "ntp.h"
#include <WiFi.h>

// ================ ПЕРВИЧНАЯ ИНИЦИАЛИЗАЦИЯ ================
void wifiInit() {
  Serial.println("[WiFi] Первичная инициализация...");
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  if(!dhcp) {
    WiFi.config(ip, gw, sn);
    Serial.printf("[WiFi] Статический IP: %s\n", ip.toString().c_str());
  } else {
    Serial.println("[WiFi] DHCP");
  }
  WiFi.begin(ssid, pass);
  Serial.printf("[WiFi] Подключение к '%s'...\n", ssid);
  uint32_t start = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(100);
  }
  if(WiFi.status() == WL_CONNECTED) {
    prevSta = true;
    logEv(EV_WIFI_ON, 0, WiFi.localIP().toString());
    Serial.printf("[WiFi] Подключено, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    prevSta = false;
    tLost = millis();
    // [FIX] Инициализируем tRetry, чтобы первая попытка была через 60 сек
    tRetry = millis();
    Serial.println("[WiFi] Не удалось подключиться за 5 сек");
  }
}

// ================ ПЕРИОДИЧЕСКОЕ УПРАВЛЕНИЕ ================
void wifiManage() {
  bool sta = (WiFi.status() == WL_CONNECTED);
  if(sta && !prevSta) {
    if(apOn) {
      WiFi.softAPdisconnect(true);
      apOn = false;
    }
    ntpSynced = false;
    ntpAttemptTime = 0;
    logEv(EV_WIFI_ON, 0, WiFi.localIP().toString());
    Serial.print("STA OK, IP: ");
    Serial.println(WiFi.localIP());
  }
  if(!sta && prevSta) {
    tLost = millis();
    logEv(EV_WIFI_OFF, 0, "");
    Serial.println("STA lost");
  }
  prevSta = sta;
  manageNTP();
  if(!sta) {
    if(!apOn && (millis() - tLost >= 15000)) {
      WiFi.softAP(AP_SSID, AP_PASS);
      apOn = true;
      Serial.println("AP on");
    }
    if(millis() - tRetry >= 60000) {
      tRetry = millis();
      bool busy = (WiFi.softAPgetStationNum() > 0) || (millis() - lastWeb < 30000);
      if(!busy) {
        WiFi.begin(ssid, pass);
        Serial.println("retry STA");
      }
    }
  }
}