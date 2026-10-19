#pragma once
#include <Arduino.h>

void mbTask();
uint16_t getHoldReg(uint16_t i);
bool setHoldReg(uint16_t i, uint16_t v);
uint16_t getInReg(uint16_t i);