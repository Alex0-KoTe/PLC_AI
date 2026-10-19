#pragma once
#include <Arduino.h>

void wifiInit();      // первичная инициализация (вызывать в setup)
void wifiManage();    // периодический вызов (в задаче)