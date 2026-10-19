#pragma once
#include <Arduino.h>
#define EV_ERR1   20    // нет охлаждения
#define EV_ERR2   21   // нагрев
#define EV_ERR3   22   // переохлаждение
#define EV_ERRCLR 23   // сброс аварии
void logEv(uint8_t code, uint8_t ch, const String& val);
void logBuildIndex();
String evName(uint8_t c);