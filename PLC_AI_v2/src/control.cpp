#include "control.h"
#include "globals.h"
#include "logger.h"

static uint32_t lastDbg = 0;

void controlTick() {
  for(int k = 0; k < 6; k++) {
    int sm = sensorMap[k];
    
    // Нет датчика — закрываем клапан (безопасность)
    if(!ok[sm]) {
      if(rel[k]) {
        rel[k] = false;
        logEv(EV_CH_OFF, k + 1, "нет датчика");  // [FIX] канал в логе — 1-based
      }
      continue;
    }
    
    float T = temp[sm];
    
    // Ошибки НЕ блокируют клапан — это только индикация
    // Клапан работает по обычной логике гистерезиса
    
    // Включение: фиксируем температуру и время старта охлаждения
    if(!rel[k] && T >= workSp[k]) {
      rel[k] = true;
      refOn[k] = T;
      onSince[k] = millis();
      logEv(EV_CH_ON, k + 1, String(T, 2));  // [FIX] 1-based
    } 
    // Выключение по гистерезису
    else if(rel[k] && T <= workSp[k] - HYST) {
      rel[k] = false;
      // Сбрасываем "нагрев" при закрытии клапана
      if(err2[k]) {
        err2[k] = false;
        errSince[k] = 0;
        logEv(EV_ERRCLR, k + 1, "нагрев сброшен (клапан закрыт)");  // [FIX] 1-based
      }
      logEv(EV_CH_OFF, k + 1, String(T, 2));  // [FIX] 1-based
    }
  }
}