#include "globals.h"

//================ ДАТЧИКИ ================
OneWire ow1(14), ow2(13), ow3(5), ow4(18), ow5(19), ow6(23);
DallasTemperature s1(&ow1), s2(&ow2), s3(&ow3), s4(&ow4), s5(&ow5), s6(&ow6);
DallasTemperature* sens[6] = {&s1, &s2, &s3, &s4, &s5, &s6};

//================ СОСТОЯНИЕ КАНАЛОВ ================
float sp[6], temp[6] = {-99.99, -99.99, -99.99, -99.99, -99.99, -99.99};
float workSp[6], step[6], deltaCool = DEF_DELTA, deltaHeat = DEF_DELTA;
uint8_t rampH[6], rampLeft[6], minuteCnt[6];
bool rampInit[6];
bool en[6], rel[6], ok[6], prevRel[6];
bool err1[6], err2[6], err3[6];
bool lockout[6], haveOff[6];
float refOn[6], refOff[6];
uint32_t onSince[6], offSince[6], lockStart[6];

//================ СЕТЬ ================
char ssid[33], pass[65];
IPAddress ip, gw, sn;
bool dhcp = false;
// [FIX] Добавлен volatile для синхронизации между ядрами
volatile bool blinkBit = false;
bool apOn = false, prevSta = false;
uint32_t lastWeb = 0, tLost = 0, tRetry = 0;

//================ СИСТЕМА ================
bool fsOk = false;
bool eeDirty = false;
uint32_t lastChangeTime = 0;
uint8_t bootReason = 0;
const char* COL[6] = {"#e6194b", "#3cb44b", "#4363d8", "#f58231", "#911eb4", "#008080"};

//================ АРХИВ ================
File archF;
uint32_t archHead = 0;
bool archOk = false;

//================ NTP ================
int8_t ntpOffsetHours = NTP_DEFAULT_OFFSET;
bool ntpEnabled = true;
bool ntpSynced = false;
uint32_t lastNtpSync = 0;
uint32_t ntpAttemptTime = 0;
Preferences preferences;

//================ MODBUS ================
WiFiServer srvA(502), srvB(503);
WiFiClient ca, cb;

//================ WEB ================
WebServer server(80);

//================ FREERTOS ================
TaskHandle_t webTaskHandle = NULL;
SemaphoreHandle_t fsMutex = NULL;

//================ УТИЛИТЫ ================
float unpackF(uint16_t hi, uint16_t lo) {
  union { uint32_t u; float f; } v;
  v.u = ((uint32_t)hi << 16) | lo;
  return v.f;
}

void packF(float f, uint16_t* hi, uint16_t* lo) {
  union { uint32_t u; float f; } v;
  v.f = f;
  *hi = v.u >> 16;
  *lo = v.u & 0xFFFF;
}

uint8_t crc8(const uint8_t* d, uint8_t n) {
  uint8_t c = 0;
  for(uint8_t i = 0; i < n; i++) {
    c ^= d[i];
    for(uint8_t j = 0; j < 8; j++)
      c = (c & 1) ? (c >> 1) ^ 0x8C : (c >> 1);
  }
  return c;
}

int8_t sensorMap[6] = {0, 1, 2, 3, 4, 5};
uint32_t errSince[6] = {0};