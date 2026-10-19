#include "settings.h"
#include "globals.h"
#include <EEPROM.h>

void defaults() {
  for(int k = 0; k < 6; k++) {
    sp[k] = DEF_SP;
    en[k] = false;
    rampH[k] = 0;
    rampLeft[k] = 0;
  }
  deltaCool = DEF_DELTA;
  deltaHeat = DEF_DELTA;
  strlcpy(ssid, DEF_SSID, 33);
  strlcpy(pass, DEF_PASS, 65);
  ip = IPAddress(192, 168, 1, 100);
  gw = IPAddress(192, 168, 1, 1);
  sn = IPAddress(255, 255, 255, 0);
  dhcp = false;
}

// ================ МАППИНГ ДАТЧИКОВ ================
void saveSensorMap() {
  preferences.begin("chmap", false);
  for(int k = 0; k < 6; k++) {
    char key[8];
    snprintf(key, sizeof(key), "sm%d", k);
    preferences.putInt(key, sensorMap[k]);
  }
  preferences.end();
  
  Serial.print("Датчики: маппинг сохранён ");
  for(int k = 0; k < 6; k++) Serial.printf("[%d->%d] ", k, sensorMap[k]);
  Serial.println();
}

void loadSensorMap() {
  preferences.begin("chmap", true);
  for(int k = 0; k < 6; k++) {
    char key[8];
    snprintf(key, sizeof(key), "sm%d", k);
    sensorMap[k] = preferences.getInt(key, k);
    if(sensorMap[k] < 0 || sensorMap[k] >= 6) sensorMap[k] = k;
  }
  preferences.end();
  
  Serial.print("Датчики: маппинг загружен ");
  for(int k = 0; k < 6; k++) Serial.printf("[%d->%d] ", k, sensorMap[k]);
  Serial.println();
}

// ================ СОХРАНЕНИЕ ================
void eeSave() {
  static uint8_t b[154];
  memset(b, 0, sizeof(b));
  
  for(int k = 0; k < 6; k++)
    if(en[k]) b[0] |= 1 << k;
  
  for(int k = 0; k < 6; k++) {
    union { float f; uint32_t u; } v;
    v.f = sp[k];
    for(int x = 0; x < 4; x++)
      b[1 + k*4 + x] = (v.u >> (8*x)) & 0xFF;
  }
  
  strlcpy((char*)b + 25, ssid, 32);
  strlcpy((char*)b + 57, pass, 64);
  
  for(int x = 0; x < 4; x++) {
    b[121 + x] = ip[x];
    b[125 + x] = gw[x];
    b[129 + x] = sn[x];
  }
  b[133] = dhcp ? 1 : 0;
  
  for(int k = 0; k < 6; k++) {
    b[134 + k] = rampH[k];
    b[140 + k] = rampLeft[k];
  }
  
  { union { float f; uint32_t u; } v; v.f = deltaCool;
    for(int x = 0; x < 4; x++) b[146 + x] = (v.u >> (8*x)) & 0xFF; }
  { union { float f; uint32_t u; } v; v.f = deltaHeat;
    for(int x = 0; x < 4; x++) b[150 + x] = (v.u >> (8*x)) & 0xFF; }
  
  EEPROM.write(0, EE_MAGIC);
  for(int i = 0; i < 154; i++)
    EEPROM.write(1 + i, b[i]);
  EEPROM.write(155, crc8(b, 154));
  EEPROM.commit();
  
  // Маппинг датчиков — отдельной функцией (исправлено)
  saveSensorMap();
}

// ================ ЗАГРУЗКА ================
void eeLoad() {
  uint8_t b[154];
  for(int i = 0; i < 154; i++)
    b[i] = EEPROM.read(1 + i);
  
  bool good = (EEPROM.read(0) == EE_MAGIC) && (EEPROM.read(155) == crc8(b, 154));
  
  if(!good) {
    defaults();
  } else {
    for(int k = 0; k < 6; k++)
      en[k] = (b[0] >> k) & 1;
    
    for(int k = 0; k < 6; k++) {
      union { float f; uint32_t u; } v;
      v.u = 0;
      for(int x = 0; x < 4; x++)
        v.u |= (uint32_t)b[1 + k*4 + x] << (8*x);
      sp[k] = v.f;
      if(sp[k] != sp[k] || sp[k] < -55.0f || sp[k] > 100.0f)
        sp[k] = DEF_SP;
    }
    
    memcpy(ssid, b + 25, 32); ssid[32] = 0;
    memcpy(pass, b + 57, 64); pass[64] = 0;
    ip = IPAddress(b[121], b[122], b[123], b[124]);
    gw = IPAddress(b[125], b[126], b[127], b[128]);
    sn = IPAddress(b[129], b[130], b[131], b[132]);
    dhcp = b[133] & 1;
    
    if(!ssid[0]) defaults();
    
    for(int k = 0; k < 6; k++) {
      rampH[k] = b[134 + k];
      rampLeft[k] = b[140 + k];
      if(rampH[k] > 99) rampH[k] = 0;
      if(rampLeft[k] > 99 || rampLeft[k] > rampH[k]) rampLeft[k] = 0;
    }
    
    union { float f; uint32_t u; } v;
    v.u = 0;
    for(int x = 0; x < 4; x++)
      v.u |= (uint32_t)b[146 + x] << (8*x);
    deltaCool = v.f;
    if(!(deltaCool >= 0.1f && deltaCool <= 10.0f))
      deltaCool = DEF_DELTA;
    
    v.u = 0;
    for(int x = 0; x < 4; x++)
      v.u |= (uint32_t)b[150 + x] << (8*x);
    deltaHeat = v.f;
    if(!(deltaHeat >= 0.1f && deltaHeat <= 10.0f))
      deltaHeat = DEF_DELTA;
  }
  
  for(int k = 0; k < 6; k++) {
    if(rampLeft[k] > 0) {
      rampInit[k] = true;
      workSp[k] = sp[k];
    } else {
      rampInit[k] = false;
      workSp[k] = sp[k];
      step[k] = 0;
    }
  }
  
  // Маппинг датчиков — отдельной функцией (исправлено)
  loadSensorMap();
}