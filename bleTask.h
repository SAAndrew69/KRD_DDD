#ifndef BLETASK_H
#define BLETASK_H

/*
Функции управления взаимодействием с BLE переферийными устройствами
*/

#include <stdint.h>
#include <string.h>
//#include "utils.h"
#include "sdk_errors.h"
#include "settings.h"

#include "bleDriver.h"


#include "FreeRTOS.h"
#include "semphr.h"

//#define BLE_UNIT_ID_INVALID (-1)  ///< значение хендла не определено

typedef int16_t conn_handle_t; ///< тип хендла соединения (максимальное количество равно NRF_BLE_LINK_COUNT)

/// @brief Виды событий для evt_handler
typedef enum
{
  BLE_TASK_CONNECTED =1,  // устройство подключено
  BLE_TASK_DISCONNECTED,  // устройство отключено
  BLE_TASK_RX,            // поступили новые данные
  BLE_TASK_TX,            // данные переданы (НЕ РЕАЛИЗОВАНО)
} bleTaskEvents_t;

/// @brief Cтруктура передаваемого сообщения (можно изменять без ограничений)
typedef struct
{
  bleTaskEvents_t   evtID;  // id события
  conn_handle_t     conn_handle; // хендл соединения
} bleTaskEvtData_t;




/**
 * @brief Инициализация задачи и запуск работы в обычном режиме
 * 
 * @return
 *  возвращает код ошибки из sdk_errors.h
*/
ret_code_t bleTaskInit(void);


/**
 * @brief Ожидание и получение сообщения из очереди
 * 
 * @return
 *  возвращает true, если данные прочитаны
 *  false - если вышли по таймауту
*/
bool bleTaskWaitEvt(bleTaskEvtData_t *evt, uint32_t wait_ms);


/**
 * @brief Очищает очередь сообщений
*/
void bleTaskEvtClear(void);


/**
 * @brief Передача бинарных данных через NUS, данные собираются в пакет
 * 
 * @param conn_handle - хендл устройства, которому нужно передать данные
 * @param buff - указатель на буфер с данными
 * @param size - размер данных в буфере
 * @param sema - семафор окончания передачи данных
 * @return
 *  Возвращает false, если данные по каким-либо причинам (переполение буфера, ни одного сборщика не подключено) не могут быть переданы
*/
bool bleTaskTxData(conn_handle_t conn_handle, uint8_t *buff, uint16_t size, SemaphoreHandle_t sema);


/**
 * @brief Передача произвольных данных через NUS с ожиданием, пока освободится место в буфере
 * 
 * @param conn_handle - хендл устройства, которому нужно передать данные
 * @param buff - указатель на буфер с данными
 * @param size - размер данных в буфере
 * @param wait_ms - максимальное время ожидания доступного места в буфере
 * @return 
 * Возвращает false, если данные по каким-либо причинам (переполение буфера, ни одного сборщика не подключено) не могут быть переданы
*/
bool bleTaskTxDataWait(conn_handle_t conn_handle, uint8_t *buff, uint16_t size, uint32_t wait_ms);


/**
 * @brief Запрос количества данных в приемном буфере
 * 
 * @return
 *  возвращает количество данных
*/
uint32_t bleGetRxDataSize(const conn_handle_t conn_handle);


/**
 * @brief Запрос количества свободного места в приемном буфере
 * 
 * @return
 *  возвращает количество данных
*/
uint32_t bleGetRxFreeSize(const conn_handle_t conn_handle);


/**
 * @brief Запрос свободного места в передающем буфере
 * 
 * @return
 *  возвращает размер свободного места
*/
uint32_t bleGetTxFreeSpace(const conn_handle_t conn_handle);


/**
 * @brief Очистка приемного буфера
*/
void bleResetRxBuff(const conn_handle_t conn_handle);


/**
 * @brief Очистка передающего буфера
*/
void bleResetTxBuff(const conn_handle_t conn_handle);


/**
 * @brief Вычитывание заданного количества данных из входного буфера
 * 
 * @param conn_handle - ID устройства
 * @param buff - буфер для размещения данных
 * @param size - количество данных для чтения
 * @param wait_ms - время ожидания данных в буфере
 * @return
 *  возвращает количество вычитанных данных
*/
uint32_t bleGetRxData(const conn_handle_t conn_handle, void *buff, uint32_t size, uint32_t wait_ms);


/**
 * @brief Отключение устройства
 * 
 * @param conn_handle - ID устройства
*/
void bleDisconnectUnit(conn_handle_t conn_handle);


/**
 * @brief Запрос хендла соединения устройства
 * 
 * @param conn_handle - ID устройства
 * @return
 *  возвращает conn_handle
*/
uint16_t bleGetConnHandle(conn_handle_t conn_handle);

#endif
