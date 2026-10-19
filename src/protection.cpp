#include "protection.h"
#include "globals.h"
#include "logger.h"

static uint32_t coolTimer[6] = {0};

void protectionTick() {
  for(int k = 0; k < 6; k++) {
    int sm = sensorMap[k];
    
    if(!ok[sm]) {
      coolTimer[k] = 0;
      continue;
    }
    
    float T = temp[sm];
    
    // ---- Нагрев (err2) ----
    if(rel[k] && (millis() - onSince[k] >= HEAT_DELAY_MS)) {
      if(T > refOn[k] + deltaHeat) {
        if(!err2[k]) {
          err2[k] = true;
          errSince[k] = millis();   // фиксируем время появления
          logEv(EV_ERR2, k + 1, String(T, 2));  // [FIX] 1-based
        }
      } else if(T <= sp[k]) {
        if(err2[k]) {
          err2[k] = false;
          errSince[k] = 0;
          logEv(EV_ERRCLR, k + 1, "нагрев сброшен");  // [FIX] 1-based
        }
      }
    }
    
    // ---- Переохлаждение (err3) ----
    if(T < sp[k] - deltaCool) {
      if(!err3[k]) {
        err3[k] = true;
        errSince[k] = millis();   // фиксируем время появления
        logEv(EV_ERR3, k + 1, String(T, 2));  // [FIX] 1-based
      }
    } else {
      if(err3[k]) {
        err3[k] = false;
        errSince[k] = 0;
        logEv(EV_ERRCLR, k + 1, "переохл. сброшено");  // [FIX] 1-based
      }
    }
    
    // ---- Нет охлаждения (err1) ----
    if(rel[k] && T > workSp[k] + 0.5) {
      if(coolTimer[k] == 0) coolTimer[k] = millis();
      if(millis() - coolTimer[k] > NO_COOL_TIME) {
        if(!err1[k]) {
          err1[k] = true;
          errSince[k] = millis();   // фиксируем время появления
          logEv(EV_ERR1, k + 1, String(T, 2));  // [FIX] 1-based
        }
      }
    } else {
      if(coolTimer[k] != 0) {
        coolTimer[k] = 0;
        if(err1[k]) {
          err1[k] = false;
          errSince[k] = 0;
          logEv(EV_ERRCLR, k + 1, "нет охл. сброшено");  // [FIX] 1-based
        }
      }
    }
    
    // ---- Автосброс ошибок через 1 час ----
    if((err1[k] || err2[k] || err3[k]) && errSince[k] > 0) {
      if(millis() - errSince[k] >= ERR_AUTO_RESET_MS) {
        if(err1[k]) logEv(EV_ERRCLR, k + 1, "нет охл. автосброс (1ч)");  // [FIX] 1-based
        if(err2[k]) logEv(EV_ERRCLR, k + 1, "нагрев автосброс (1ч)");  // [FIX] 1-based
        if(err3[k]) logEv(EV_ERRCLR, k + 1, "переохл. автосброс (1ч)");  // [FIX] 1-based
        err1[k] = false;
        err2[k] = false;
        err3[k] = false;
        errSince[k] = 0;
      }
    }
  }
}

void clearAlarm(int k) {
  if(k < 0 || k >= 6) return;
  err1[k] = false;
  err2[k] = false;
  err3[k] = false;
  errSince[k] = 0;
  coolTimer[k] = 0;
  logEv(EV_ERRCLR, k + 1, "ручной сброс");  // [FIX] 1-based
}

void clearAlarms() {
  for(int k = 0; k < 6; k++) {
    if(err1[k] || err2[k] || err3[k]) clearAlarm(k);
  }
}