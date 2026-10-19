#pragma once
#include <Arduino.h>

void protectionTick();
void clearAlarm(int k);   // сброс одного канала
void clearAlarms();       // сброс всех каналов