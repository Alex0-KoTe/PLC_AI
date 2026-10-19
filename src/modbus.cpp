#include "modbus.h"
#include "globals.h"
#include "logger.h"
#include "ramp.h"
#include "protection.h"

uint16_t getHoldReg(uint16_t i) {
  if(i == 0) {
    uint16_t w = blinkBit ? 1 : 0;
    for(int k = 0; k < 6; k++) {
      if(en[k])  w |= 1 << (2 + 2*k);  // чётные биты (2,4,6,8,10,12): разрешения en[]
      if(rel[k]) w |= 1 << (1 + 2*k);  // нечётные биты (1,3,5,7,9,11): состояния реле rel[]
    }
    return w;
  }
  if(i <= 12) {
    int ch = (i - 1) / 2;
    uint16_t hi, lo;
    packF(sp[ch], &hi, &lo);
    return ((i - 1) & 1) ? lo : hi;
  }
  if(i <= 18) return rampH[i - 13];
  if(i <= 24) return rampLeft[i - 19];
  if(i <= 36) {
    int ch = (i - 25) / 2;
    uint16_t hi, lo;
    packF(workSp[ch], &hi, &lo);
    return ((i - 25) & 1) ? lo : hi;
  }
  if(i <= 38) {
    uint16_t hi, lo;
    packF(deltaCool, &hi, &lo);
    return ((i - 37) & 1) ? lo : hi;
  }
  if(i == 39) {
    uint16_t w = 0;
    for(int k = 0; k < 6; k++) {
      if(rampLeft[k] > 0) w |= 1 << k;
      if(err1[k])         w |= 1 << (6 + k);
    }
    return w;
  }
  if(i == 40) {
    uint16_t w = 0;
    for(int k = 0; k < 6; k++) {
      if(err2[k]) w |= 1 << k;
      if(err3[k]) w |= 1 << (6 + k);
    }
    return w;
  }
  uint16_t hi, lo;
  packF(deltaHeat, &hi, &lo);
  return ((i - 41) & 1) ? lo : hi;
}

bool setHoldReg(uint16_t i, uint16_t v) {
  if(i == 0) {
    // Маска 0x1554 = 0001 0101 0101 0100 — биты 2,4,6,8,10,12 (разрешения en[])
    uint16_t nv = (getHoldReg(0) & ~0x1554) | (v & 0x1554);
    for(int k = 0; k < 6; k++) {
      bool e = (nv >> (2 + 2*k)) & 1;
      if(en[k] != e) {
        en[k] = e;
        clearAlarm(k);
        logEv(e ? EV_CH_ON : EV_CH_OFF, k + 1, "");
        eeDirty = true;
        lastChangeTime = millis();
      }
    }
    return true;
  }
  if(i <= 12) {
    int ch = (i - 1) / 2;
    uint16_t hi, lo;
    packF(sp[ch], &hi, &lo);
    if((i - 1) & 1) lo = v; else hi = v;
    float nf = unpackF(hi, lo);
    if(!(nf >= -55.0f && nf <= 100.0f)) return false;
    if(nf == sp[ch]) return true;
    sp[ch] = nf;
    
    // [FIX] МГНОВЕННО обновляем workSp, если рампа не активна
    // Без этой строки реле реагировало на изменение уставки только
    // при следующем вызове rampTickMinute() — раз в 60 секунд!
    if(rampH[ch] <= 0) workSp[ch] = nf;
    
    logEv(EV_SP, ch + 1, String(nf, 1));
    if(rampLeft[ch] > 0 && !rampInit[ch] && ok[ch])
      step[ch] = (temp[ch] - sp[ch]) / ((float)rampLeft[ch] * 60.0f);
    eeDirty = true;
    lastChangeTime = millis();
    return true;
  }
  if(i <= 18) {
    uint8_t h = v > 99 ? 99 : (uint8_t)v;
    if(h != rampH[i - 13]) rampSet(i - 13, h);
    return true;
  }
  if(i <= 36) return true;  // workSp — только чтение
  if(i <= 38) {
    int ch = i - 37;
    uint16_t hi, lo;
    packF(deltaCool, &hi, &lo);
    if(ch) lo = v; else hi = v;
    float nf = unpackF(hi, lo);
    if(!(nf >= 0.1f && nf <= 10.0f)) return false;
    if(nf == deltaCool) return true;
    deltaCool = nf;
    eeDirty = true;
    lastChangeTime = millis();
    return true;
  }
  if(i <= 40) return true;  // регистры 39, 40 — только чтение
  if(i <= 42) {
    int ch = i - 41;
    uint16_t hi, lo;
    packF(deltaHeat, &hi, &lo);
    if(ch) lo = v; else hi = v;
    float nf = unpackF(hi, lo);
    if(!(nf >= 0.1f && nf <= 10.0f)) return false;
    if(nf == deltaHeat) return true;
    deltaHeat = nf;
    eeDirty = true;
    lastChangeTime = millis();
    return true;
  }
  return true;
}

uint16_t getInReg(uint16_t i) {
  // [FIX] Учитываем маппинг датчиков
  int sm = sensorMap[i / 2];
  uint16_t hi, lo;
  packF(temp[sm], &hi, &lo);
  return (i & 1) ? lo : hi;
}

static void exc(uint8_t* o, int& n, uint16_t tid, uint8_t uid, uint8_t fc, uint8_t e) {
  o[0] = tid >> 8; o[1] = tid; o[2] = 0; o[3] = 0;
  o[4] = 0; o[5] = 3; o[6] = uid; o[7] = fc | 0x80; o[8] = e;
  n = 9;
}

// [FIX] Ответ на FC 0x06 (Write Single Register) — эхо запроса, 12 байт
static void echo6(uint8_t* o, int& n, uint16_t tid, uint8_t uid, const uint8_t* p) {
  o[0] = tid >> 8; o[1] = tid; o[2] = 0; o[3] = 0;
  o[4] = 0; o[5] = 6; o[6] = uid;
  // payload FC 0x06: FC(1) + addr(2) + value(2) = 5 байт
  for(int i = 0; i < 5; i++) o[7 + i] = p[i];
  n = 12;  // [FIX] Было 13 — это была ошибка! MBAP(7) + PDU(5) = 12 байт
}

// [FIX] Ответ на FC 0x10 (Write Multiple Registers) — эхо адреса и количества, 12 байт
static void echo10(uint8_t* o, int& n, uint16_t tid, uint8_t uid, const uint8_t* p) {
  o[0] = tid >> 8; o[1] = tid; o[2] = 0; o[3] = 0;
  o[4] = 0; o[5] = 6; o[6] = uid;
  // payload FC 0x10: FC(1) + addr(2) + qty(2) = 5 байт (без byte_count)
  for(int i = 0; i < 5; i++) o[7 + i] = p[i];
  n = 12;
}

static void handleMB(WiFiClient &c) {
  if(c.available() < 7) return;
  
  uint8_t h[7];
  if(c.readBytes((char*)h, 7) != 7) return;
  
  uint16_t tid = (h[0] << 8) | h[1];
  uint16_t len = (h[4] << 8) | h[5];
  uint8_t uid = h[6];
  
  int pl = len - 1;
  if(pl < 1 || pl > 246) { c.stop(); return; }
  
  // [FIX] Убран static — теперь буферы локальные, нет гонки при двух клиентах
  uint8_t p[246];
  if(c.readBytes((char*)p, pl) != pl) return;
  
  if(uid != 1 && uid != 0) return;
  
  uint8_t o[300];
  int n = 0;
  uint8_t fc = p[0];
  uint16_t a = (p[1] << 8) | p[2];
  uint16_t q = (p[3] << 8) | p[4];
  
  if(fc == 0x03 || fc == 0x04) {
    uint16_t mx = (fc == 0x03) ? HREG_N : IN_N;
    if(q == 0 || a + q > mx) {
      exc(o, n, tid, uid, fc, 2);
    } else {
      o[0] = tid >> 8; o[1] = tid; o[2] = 0; o[3] = 0;
      o[4] = 0; o[5] = 3 + q*2; o[6] = uid; o[7] = fc; o[8] = q*2;
      for(int i = 0; i < q; i++) {
        uint16_t v = (fc == 0x03) ? getHoldReg(a + i) : getInReg(a + i);
        o[9 + 2*i] = v >> 8;
        o[10 + 2*i] = v & 0xFF;
      }
      n = 9 + q*2;
    }
  }
  else if(fc == 0x06) {
    if(a >= HREG_N) exc(o, n, tid, uid, fc, 2);
    else if(!setHoldReg(a, (p[3] << 8) | p[4])) exc(o, n, tid, uid, fc, 3);
    else echo6(o, n, tid, uid, p);
  }
  else if(fc == 0x10) {
    if(a + q > HREG_N || p[5] != q*2)
      exc(o, n, tid, uid, fc, (a + q > HREG_N) ? 2 : 3);
    else {
      bool bad = false;
      for(int i = 0; i < q; i++)
        if(!setHoldReg(a + i, (p[6 + 2*i] << 8) | p[7 + 2*i]))
          bad = true;
      if(bad) exc(o, n, tid, uid, fc, 3);
      // [FIX] Используем echo10 вместо echo6
      else echo10(o, n, tid, uid, p);
    }
  }
  else {
    exc(o, n, tid, uid, fc, 1);
  }
  
  if(n) c.write(o, n);
}

void mbTask() {
  if(!ca || !ca.connected()) {
    if(srvA.hasClient()) ca = srvA.accept();
  }
  if(!cb || !cb.connected()) {
    if(srvB.hasClient()) cb = srvB.accept();
  }
  if(ca && ca.connected()) handleMB(ca);
  if(cb && cb.connected()) handleMB(cb);
}