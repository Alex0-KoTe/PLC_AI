#pragma once
#include <Arduino.h>
// Инициализация архива
void archInit();
// Запись данных (вызывать каждые 30 секунд)
void archWrite(uint32_t t, const int16_t* v);
// Чтение данных за конкретный день
// [FIX] Возвращает количество прочитанных БАЙТ (а не записей)
int archReadDay(uint32_t dateNum, uint8_t* data);
// Получить список доступных дней (массив дат в формате YYYYMMDD)
int archGetDayList(uint32_t* days, int maxCount);
// Проверка и очистка при нехватке места
void archCleanup();
// Получить дату в формате YYYYMMDD из timestamp
uint32_t archDateFromTime(uint32_t timestamp);
// Проверить существует ли файл за указанную дату
bool archDayExists(uint32_t dateNum);
// Получить размер свободного места
size_t archGetFreeSpace();