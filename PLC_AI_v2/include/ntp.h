#pragma once
#include <Arduino.h>

// Основные функции
bool initNTP();
void manageNTP();
String getFormattedTime();
String getShortTime();

// Настройки
void saveNtpSettings();
void loadNtpSettings();
void applyTimezone();

// Время
void timeInit();
void timeTick();
void saveCurrentTime();
time_t getDefaultTime();

// НОВОЕ: ручная установка времени (лольные компоненты)
bool setManualTime(int year, int month, int day, int hour, int min, int sec);