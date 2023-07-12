#ifndef SYS_H
#define SYS_H

/*
Этот модуль используется для объявления обработчиков прерываний интерфейсов и общих функций FreeRTOS
*/

#include <stdbool.h>
#include <stdint.h>


typedef void (*TPTR)(void);
typedef void (*TPTA)(void *args);





void sysSetLoggerAppIdleHook(TPTR hook); // установка обработчика от модуля logger в системную функцию vApplicationIdleHook()
void sysSetSpim3Hook(TPTA hookA, void *args); // установка обработчика от шины SPIM3

void systemReset(void); // перезагрузка системы

uint16_t WDT_Run(uint32_t time); // запуск WDT (time - в мс)
void WDT_Reset(void); // сброс wdt-таймера

#endif
