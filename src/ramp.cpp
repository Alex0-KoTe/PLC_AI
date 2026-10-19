#include "ramp.h"
#include "globals.h"
#include "logger.h"

void rampSet(uint8_t k, uint8_t hours) {
  rampH[k] = hours;
  int sm = sensorMap[k];   // учитываем маппинг
  
  if(hours > 0) {
    rampLeft[k] = hours;
    minuteCnt[k] = 0;
    if(ok[sm]) {
      rampInit[k] = false;
      workSp[k] = temp[sm];
      step[k] = (temp[sm] - sp[k]) / ((float)hours * 60.0f);
    } else {
      rampInit[k] = true;
    }
    logEv(EV_RAMP_ON, k + 1, String(hours));
  } else {
    rampLeft[k] = 0;
    step[k] = 0;
    workSp[k] = sp[k];
  }
  eeDirty = true;
  lastChangeTime = millis();
}

void rampTickMinute() {
  for(int k = 0; k < 6; k++) {
    int sm = sensorMap[k];   // учитываем маппинг
    
    // Рампа ждала датчика и он появился — стартуем
    if(rampInit[k] && ok[sm] && rampLeft[k] > 0) {
      rampInit[k] = false;
      workSp[k] = temp[sm];
      step[k] = (temp[sm] - sp[k]) / ((float)rampLeft[k] * 60.0f);
    }
    
    if(rampLeft[k] == 0) {
      workSp[k] = sp[k];
      continue;
    }
    if(rampInit[k]) continue;
    
    workSp[k] -= step[k];
    if(step[k] >= 0 && workSp[k] < sp[k]) workSp[k] = sp[k];
    if(step[k] < 0 && workSp[k] > sp[k]) workSp[k] = sp[k];
    
    if(++minuteCnt[k] >= 60) {
      minuteCnt[k] = 0;
      rampLeft[k]--;
      logEv(EV_RAMP_HR, k + 1, String(rampLeft[k]));
      if(rampLeft[k] <= 0) {
        rampLeft[k] = 0;
        step[k] = 0;
        workSp[k] = sp[k];
      } else if(ok[sm]) {
        step[k] = (temp[sm] - sp[k]) / ((float)rampLeft[k] * 60.0f);
      }
      eeDirty = true;
      lastChangeTime = millis();
    }
  }
}