/**
 * Модуль реализует функции для работы с микросхемами АЦП серии ADS129x
 * 
 * 09.07.2023 - Реализованы базовые функции для работы микросхемами АЦП
 * 
 * ОСОБЕННОСТИ
 * - все функции блокируют поток до их завершения
*/


#include "ads129x.h"
#include "spim_freertos.h"
#include "errors.h"

#include "FreeRTOS.h"







// структура описания устройства на шине SPI
typedef struct {
  uint8_t csPin;
  uint8_t spiDevID;
  
} ads129x_t;






/*
* Добавление устройства на шину SPI
*
* spiDevID - ID интерфейса SPI (получается после инициализации SPI)
* csPin - номер выхода чип-селекта (если не используется, то SPI_PIN_NOT_USED)
* handle - хендл устройства на шине SPI
* Возврат:
* код ошибки из errors.h или ERR_NOERROR
*/
uint16_t ads129x_add(uint8_t spiDevID, uint8_t csPin, ads129x_handle_t *handle)
{
  ASSERT(handle != NULL);
  
  ads129x_t *hdl = (ads129x_t *)pvPortMalloc(sizeof(ads129x_t));
  if(hdl == NULL) return ERR_OUT_OF_MEMORY;
  
  hdl->csPin = csPin;
  hdl->spiDevID = spiDevID;
  
  *handle = (uint32_t *)hdl;
  return ERR_NOERROR;
}


/*
* Удаление устройства с шины SPI
*
* handle - хендл устройства на шине SPI
*/
void ads129x_remove(ads129x_handle_t handle)
{
  vPortFree(handle);
}


/*
* Отправка команды
*
* handle - хендл устройства на шине SPI
* cmd - код команды
* timeout_ms - таймаут ожидания доступа к интерфейсу
* Возврат:
* код ошибки из errors.h или ERR_NOERROR
*/
uint16_t ads129x_cmd(ads129x_handle_t handle, uint8_t cmd, uint32_t timeout_ms)
{
  ASSERT(handle != NULL);
  
  ads129x_t *hdl = (ads129x_t *)handle;
  
  spi_queue_t spi_task = {
    .cmd = cmd,
    .cmdBuffLen = 1,
    .csPin = hdl->csPin
  };
        
  return spiTaskExecute(hdl->spiDevID, &spi_task, pdMS_TO_TICKS(timeout_ms));
}


/*
* Чтение регистров
*
* handle - хендл устройства на шине SPI
* reg_addr - адрсе регистра
* cnt - количество регистров для чтения
* rx_data - указатель на буфер
* timeout_ms - таймаут ожидания доступа к интерфейсу
* Возврат:
* код ошибки из errors.h или ERR_NOERROR
*/
uint16_t ads129x_read_regs(ads129x_handle_t handle, uint8_t reg_addr, uint8_t cnt, uint8_t *rx_data, uint32_t timeout_ms)
{
  ASSERT((cnt != 0)||(rx_data != NULL)||(handle != NULL));
  
  ads129x_t *hdl = (ads129x_t *)handle;
  
  uint8_t cmdBuff[2];
  cmdBuff[0] = ADS129X_CMD_RREG + (reg_addr & 0x1F);
  cmdBuff[1] = cnt - 1;
  spi_queue_t spi_task = {
    .cmdBuff = (uint8_t *)cmdBuff,
    .cmdBuffLen = 2,
    .rxData = rx_data,
    .rxDataLen = cnt,
    .csPin = hdl->csPin
  };
        
  return spiTaskExecute(hdl->spiDevID, &spi_task, pdMS_TO_TICKS(timeout_ms));
}


/*
* Запись регистров
*
* handle - хендл устройства на шине SPI
* reg_addr - адрсе регистра
* cnt - количество регистров для записи
* tx_data - указатель на буфер
* timeout_ms - таймаут ожидания доступа к интерфейсу
* Возврат:
* код ошибки из errors.h или ERR_NOERROR
*/
uint16_t ads129x_write_regs(ads129x_handle_t handle, uint8_t reg_addr, uint8_t cnt, uint8_t *tx_data, uint32_t timeout_ms)
{
  ASSERT((cnt != 0)||(tx_data != NULL)||(handle != NULL));
  
  ads129x_t *hdl = (ads129x_t *)handle;

  uint8_t cmdBuff[2];
  cmdBuff[0] = ADS129X_CMD_WREG + (reg_addr & 0x1F);
  cmdBuff[1] = cnt - 1;
  spi_queue_t spi_task = {
    .cmdBuff = (uint8_t *)cmdBuff,
    .cmdBuffLen = 2,
    .txData = tx_data,
    .txDataLen = cnt,
    .csPin = hdl->csPin
  };
        
  return spiTaskExecute(hdl->spiDevID, &spi_task, pdMS_TO_TICKS(timeout_ms));
}


/*
* Запись регистра
*
* handle - хендл устройства на шине SPI
* reg_addr - адрсе регистра
* val - значение для записи
* timeout_ms - таймаут ожидания доступа к интерфейсу
* Возврат:
* код ошибки из errors.h или ERR_NOERROR
*/
uint16_t ads129x_write_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t val, uint32_t timeout_ms)
{
  return ads129x_write_regs(handle, reg_addr, 1, (uint8_t *)&val, timeout_ms);
}


/*
* Чтение данных (формат данных описан в документации на ads129x)
* Данные и статус имеют 24 разряда.
*
* handle - хендл устройства на шине SPI
* rx_data - указатель на буфер данных
* timeout_ms - таймаут ожидания доступа к интерфейсу
* Возврат:
* код ошибки из errors.h или ERR_NOERROR
*/
uint16_t ads129x_read_data(ads129x_handle_t handle, ads129x_data_t *rx_data, uint32_t timeout_ms)
{
  ASSERT((rx_data != NULL)||(handle != NULL));
  
  // ads129x_data_t data;
  
  ads129x_t *hdl = (ads129x_t *)handle;
  
  spi_queue_t spi_task = {
    .cmd = ADS129X_CMD_RDATA,
    .cmdBuffLen = 1,
    .rxData = (uint8_t *)&data,
    .rxDataLen = sizeof(data),
    .csPin = hdl->csPin
  };
        
  uint16_t err = spiTaskExecute(hdl->spiDevID, &spi_task, pdMS_TO_TICKS(timeout_ms));
  
  if(err != ERR_NOERROR) return err;
  
  // преобразую данные к 32 битам
  // rx_data->status = data.status.val;
  
  // for(uint8_t i=0; i < ADS129X_CH_CNT; i++)
  // {
  //   if(data.ch[i].val & (1UL << 23)) rx_data->ch[i] = (0xFFUL << 24) + data.ch[i].val;
  //   else rx_data->ch[i] = data.ch[i].val;
  // }
  
  return ERR_NOERROR;
}






