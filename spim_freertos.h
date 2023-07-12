#ifndef SPIM_FREERTOS_H
#define SPIM_FREERTOS_H
/*
Модуль мастера SPI с easyDMA в виде объекта класса для работы с разными интерфейсами

*/

// НАСТРОЙКИ МОДУЛЯ ************************************
#define SPIM_INSTANCE_CNT   2     // максимальное количество интерфейсов SPIM 
// *****************************************************


#include "nrf.h"
#include <stdbool.h>
#include <stdint.h>







/**
 * @brief This value can be provided instead of a pin number for signals MOSI,
 *        MISO, and Slave Select to specify that the given signal is not used and
 *        therefore does not need to be connected to a pin.
 */
#define SPI_PIN_NOT_USED  0xFF


typedef struct
{ // структура очереди задач для SPIM
  uint16_t        delay_ms; // задержка возврата управления (если =0, то задержки нет)
  uint8_t         *cmdBuff; // указатель на буфер команды
  uint8_t         cmd;      // код команды для одиночных команды
  uint8_t         cmdBuffLen; // длина команды (если =1 и cmdBuff=0, то код команды берется из cmd)
  uint8_t         *rxData;  // указатель буфер данных для чтения
  uint16_t        rxDataLen; // сколько данных нужно прочитать
  uint8_t         *txData;  // указатель буфер данных для записи
  uint16_t        txDataLen; // сколько данных нужно записать
  uint8_t         csPin;    // номер пина chip select 
} spi_queue_t;


/*
ВХОД:
spim - указатель на адрес интерфейса SPI (поддерживаются SPIM2 и SPIM3)
irqn - номер прерывания
irqPrior - приоритет прерывания
boudrate - скорость обмена по SPI (максималка для SPIM2 - 8M, для SPIM3 - 32M)
ВЫХОД:
devID - ID интерфейса
возвращает код ошибки
*/
uint16_t spiInit(uint8_t *devID, NRF_SPIM_Type *spim, IRQn_Type irqn, uint8_t irqPrior, uint32_t freq); // начальная инициализация

uint16_t spiDeInit(uint8_t devID);


/*
* Запускает исполнение задачи обмена данными по SPI
* devID - ID интерфейса
* task - указатель на задачу для SPI
* wait_ticks - максимальное время ожидания начала выполнения задачи, если интерфейс занят
* ВЫХОД: код ошибки из errors.h
*/
uint16_t spiTaskExecute(uint8_t devID, spi_queue_t *task, uint32_t wait_ticks); // помещает новую задачу (прием или передача данных по SPI) в буфер и запускает ее исполнение


















#endif
