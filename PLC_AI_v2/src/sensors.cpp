#include "sensors.h"
#include "globals.h"
#include "ramp.h"

static bool waitingForSensors = false;
static uint32_t sensorRequestTime = 0;

void sensorsInit() {
  for(int i = 0; i < 6; i++) {
    sens[i]->begin();
    sens[i]->setResolution(10);
  }
}

void sensorsRequest() {
  for(int i = 0; i < 6; i++) {
    sens[i]->requestTemperatures();
  }
  sensorRequestTime = millis();
  waitingForSensors = true;
}

bool sensorsReady() {
  if(!waitingForSensors) return false;
  // ОПТИМИЗАЦИЯ: 250 мс для 10-битного режима
  if(millis() - sensorRequestTime >= 250) {
    waitingForSensors = false;
    return true;
  }
  return false;
}

void sensorsRead() {
  for(int i = 0; i < 6; i++) {
    // [FIX] Добавлен маппинг: i — канал, sm — датчик
    int sm = sensorMap[i];
    float t = sens[sm]->getTempCByIndex(0);
    ok[sm] = (!isnan(t) && t > -100);
    temp[sm] = ok[sm] ? t : -99.99f;
    
    // [FIX] Используем ok[sm] и temp[sm] вместо ok[i] и temp[i]
    if(rampInit[i] && ok[sm] && rampLeft[i] > 0) {
      rampInit[i] = false;
      workSp[i] = temp[sm];
      step[i] = (temp[sm] - sp[i]) / ((float)rampLeft[i] * 60.0f);
      minuteCnt[i] = 0;
    }
    if(!haveOff[i] && ok[sm]) {
      refOff[i] = temp[sm];
      haveOff[i] = true;
    }
  }
}