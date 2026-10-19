#pragma once
// [FIX] Убраны лишние пробелы в #include
#include <Arduino.h>
#include <IPAddress.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WebServer.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

//================ ДАТЧИКИ ================
extern OneWire ow1, ow2, ow3, ow4, ow5, ow6;
extern DallasTemperature s1, s2, s3, s4, s5, s6;
extern DallasTemperature* sens[6];

//================ СОСТОЯНИЕ КАНАЛОВ ================
extern float sp[6], temp[6];
extern float workSp[6], step[6];
extern float deltaCool, deltaHeat;
extern uint8_t rampH[6], rampLeft[6], minuteCnt[6];
extern bool rampInit[6];
extern bool en[6], rel[6], ok[6], prevRel[6];
extern bool err1[6], err2[6], err3[6];
extern bool lockout[6], haveOff[6];
extern float refOn[6], refOff[6];
extern uint32_t onSince[6], offSince[6], lockStart[6];
// [FIX] Убраны дубли extern float refOn[6]; и extern uint32_t onSince[6];
extern uint32_t errSince[6];   // время появления ошибки (для автосброса)

//================ СЕТЬ ================
extern char ssid[33], pass[65];
extern IPAddress ip, gw, sn;
extern bool dhcp;
// [FIX] Добавлен volatile для синхронизации между ядрами
extern volatile bool blinkBit;
extern bool apOn, prevSta;
extern uint32_t lastWeb, tLost, tRetry;

//================ СИСТЕМА ================
extern bool fsOk;
extern bool eeDirty;
extern uint32_t lastChangeTime;
extern uint8_t bootReason;
extern const char* COL[6];

//================ АРХИВ ================
extern File archF;
extern uint32_t archHead;
extern bool archOk;

//================ NTP ================
extern int8_t ntpOffsetHours;
extern bool ntpSynced;
extern bool ntpEnabled;
extern uint32_t lastNtpSync;
extern uint32_t ntpAttemptTime;
extern Preferences preferences;

//================ MODBUS ================
extern WiFiServer srvA, srvB;
extern WiFiClient ca, cb;

//================ WEB ================
extern WebServer server;

//================ FREERTOS ================
extern TaskHandle_t webTaskHandle;
extern SemaphoreHandle_t fsMutex;
// [FIX] Мьютекс общих данных каналов (Core 0 web/modbus vs Core 1 control)
extern SemaphoreHandle_t stateMutex;

//================ УТИЛИТЫ ================
float unpackF(uint16_t hi, uint16_t lo);
void  packF(float f, uint16_t* hi, uint16_t* lo);
uint8_t crc8(const uint8_t* d, uint8_t n);

//================ МАППИНГ ДАТЧИКОВ ================
extern int8_t sensorMap[6];    // sensorMap[k] = индекс датчика для канала k
void loadSensorMap();
void saveSensorMap();