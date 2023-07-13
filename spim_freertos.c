/*
Модуль мастера SPIM с easyDMA

08.07.2023 - Реализовал и отладил прием-передачу через SPIM через очередь сообщений с поддержкой CS (chip select)
             Работа с CS реализована программно

ОГРАНИЧЕНИЯ
- модуль работает только с портом SPIM3 (настраивается по дейфайнам из board_v1-0)
- SPIM работает в режиме SPI_MODE_1, SPI_BIT_ORDER_MSB_FIRST

ОСОБЕННОСТИ
- пока идет передача данных, прием данных не осуществляется (все, что будет принято, дропается)
- если код ошибки от модуля, то к нем добавляется ERR_SPIM_MODULE, если от Softdevice, то ничего не добавляется

*/

#include "spim_freertos.h"
//#include "nrf_sdh.h"
//#include "nrf_sdh_soc.h"
#include "nrf_delay.h"
#include "sys.h"
#include "errors.h"
#include "settings.h"
#include "custom_board.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"



// НАСТРОЙКИ МОДУЛЯ ************************************
#ifndef SPIM_TASK_TIMEOUT_MS
#define SPIM_TASK_TIMEOUT_MS      1000  // максимальное время выполнения одной задачи по SPI (при передаче больших объемов на маленькой скорости надо будет увеличивать)
#endif // SPIM_TASK_TIMEOUT_MS

#if(RTTLOG_EN)
#include "logger_freertos.h"
#define RTT_LOG_EN                1     // включить лог через RTT
#define TAG                     "SPIM: "
#else
#define RTT_LOG_EN 0
#endif // RTTLOG_EN
// *****************************************************

#if(RTT_LOG_EN)
#define RTT_LOG_INFO(...)       \
                                \
  {                             \
    NRF_LOG_INFO(TAG +__VA_ARGS__);  \
  }
#else
  #define RTT_LOG_INFO(...) {}
#endif // RTT_LOG_EN


    
    
    
typedef enum
{ // режимы работы
  SPIM_MODE_CMD,  // передача кода команды
  SPIM_MODE_TXD, // передача данных
  SPIM_MODE_RXD, // прием данных
} mode_t;

    
/**
 * @brief SPI modes.
 */
typedef enum
{
    SPI_MODE_0, ///< SCK active high, sample on leading edge of clock.
    SPI_MODE_1, ///< SCK active high, sample on trailing edge of clock.
    SPI_MODE_2, ///< SCK active low, sample on leading edge of clock.
    SPI_MODE_3  ///< SCK active low, sample on trailing edge of clock.
} spi_mode_t;

/**
 * @brief SPI bit orders.
 */
typedef enum
{
    SPI_BIT_ORDER_MSB_FIRST = SPI_CONFIG_ORDER_MsbFirst, ///< Most significant bit shifted out first.
    SPI_BIT_ORDER_LSB_FIRST = SPI_CONFIG_ORDER_LsbFirst  ///< Least significant bit shifted out first.
} spi_bit_order_t;











__STATIC_INLINE void spi_configure(NRF_SPIM_Type * p_reg,
                                       spi_mode_t spi_mode,
                                       spi_bit_order_t spi_bit_order)
{
    uint32_t config = (spi_bit_order == SPI_BIT_ORDER_MSB_FIRST ?
        SPI_CONFIG_ORDER_MsbFirst : SPI_CONFIG_ORDER_LsbFirst);
    switch (spi_mode)
    {
    default:
    case SPI_MODE_0:
        config |= (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos) |
                  (SPI_CONFIG_CPHA_Leading    << SPI_CONFIG_CPHA_Pos);
        break;

    case SPI_MODE_1:
        config |= (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos) |
                  (SPI_CONFIG_CPHA_Trailing   << SPI_CONFIG_CPHA_Pos);
        break;

    case SPI_MODE_2:
        config |= (SPI_CONFIG_CPOL_ActiveLow  << SPI_CONFIG_CPOL_Pos) |
                  (SPI_CONFIG_CPHA_Leading    << SPI_CONFIG_CPHA_Pos);
        break;

    case SPI_MODE_3:
        config |= (SPI_CONFIG_CPOL_ActiveLow  << SPI_CONFIG_CPOL_Pos) |
                  (SPI_CONFIG_CPHA_Trailing   << SPI_CONFIG_CPHA_Pos);
        break;
    }
    p_reg->CONFIG = config;
}


typedef struct
{
  NRF_SPIM_Type         *spim; // указатель на интерфейс
  mode_t                mode;  // режим работы
  spi_queue_t           *q;    // укатель на текущее задание
  IRQn_Type             IRQn;  // номер вектора прерывания
  uint32_t              irqPrior; // приоритет прерывания от SPIM
  uint8_t               isInited:1; // флаг, что интерфейс проинициализирован
  uint8_t               fake_buff[1]; // фейковый буфер для работы SPIM (используется, когда нет необходимости передавать или принимать данные)
  SemaphoreHandle_t     mutex;  // мьютекс занятости устройства
  SemaphoreHandle_t     irqSema; // бинарный семафор выхода из прерывания
} spim_instance_t;



static spim_instance_t spim_instance[SPIM_INSTANCE_CNT]; // хранение описаний интерфейсов


static void irqHandler(void *instance); // обработчик прерываний
static bool spiStartWriteRead(void *instance, uint32_t wait_ticks); // запуск операции записи/чтения






static void SPIM_TransactionStop(void *instance)
{ // отработка окончания транзакции
  
    spim_instance_t *dev = (spim_instance_t *)instance;
  
    // транзакция успешно завершена
    dev->spim->INTENCLR = 0xFFFFFFFF; // запрещаем все прерывания
    
    if(dev->q->csPin != SPI_PIN_NOT_USED)
    { // используется CS
      // CS = 1
      if(dev->q->csPin >= 32)
        NRF_P1->OUTSET = (1 << (dev->q->csPin - 32));
      else NRF_P0->OUTSET = (1 << dev->q->csPin);
    }
    // устанавливаю семафор выхода из прерывания
//    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    UNUSED_RETURN_VALUE(xSemaphoreGiveFromISR(dev->irqSema, &xHigherPriorityTaskWoken));
    UNUSED_RETURN_VALUE(xSemaphoreGiveFromISR(dev->irqSema, NULL));
//    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void irqHandler(void *instance)
{ // обработчик прерываний
  
  spim_instance_t *dev = (spim_instance_t *)instance;
  
  if((dev->spim->EVENTS_STOPPED == 1) && (dev->spim->INTENSET & SPIM_INTENSET_STOPPED_Msk))
  { // конец транзакции
    SPIM_TransactionStop(instance);   
    return;
  }
  
  if( (dev->spim->EVENTS_END == 1) && (dev->spim->INTENSET & SPIM_INTENSET_END_Msk) )
  { // закончили передачу или прием
    dev->spim->INTENCLR = 0xFFFFFFFF;
    dev->spim->EVENTS_END = 0;
    dev->spim->EVENTS_STOPPED = 0;
    
    switch(dev->mode)
    {
      case SPIM_MODE_CMD: // закончилась передача команды
        if(dev->q->txDataLen)
        { // есть данные для передачи
          dev->mode = SPIM_MODE_TXD;
          dev->spim->TXD.PTR = (uint32_t)dev->q->txData;
          dev->spim->TXD.MAXCNT = dev->q->txDataLen;
          dev->spim->INTENSET = SPIM_INTENSET_END_Msk;
          dev->spim->TASKS_START = 1; // стартую передачу
        }else if(dev->q->rxDataLen){ // есть данные для приема
          dev->spim->TXD.MAXCNT = 0;
          dev->spim->RXD.MAXCNT = dev->q->rxDataLen;
          dev->spim->INTENSET = SPIM_INTENSET_END_Msk;
          dev->mode = SPIM_MODE_RXD;
          dev->spim->TASKS_START = 1; // стартую прием
        }else{ // принимать ничего не надо
          dev->spim->INTENSET = SPIM_INTENSET_STOPPED_Msk; // разрешаю прерывание по окончанию транзакции
          dev->spim->TASKS_STOP = 1; // нет данных ни для приема, ни для передачи: конец транзакции
        }
      break;
      
      case SPIM_MODE_TXD: // закончилась передача данных
        if(dev->q->rxDataLen){ // есть данные для приема
          dev->spim->TXD.MAXCNT = 0;
          dev->spim->RXD.MAXCNT = dev->q->rxDataLen;
          dev->spim->INTENSET = SPIM_INTENSET_END_Msk;
          dev->mode = SPIM_MODE_RXD;
          dev->spim->TASKS_START = 1; // стартую прием
        }else{ // принимать ничего не надо
          dev->spim->INTENSET = SPIM_INTENSET_STOPPED_Msk; // разрешаю прерывание по окончанию транзакции
          dev->spim->TASKS_STOP = 1; // нет данных ни для приема, ни для передачи: конец транзакции
        }
      break;
      
      case SPIM_MODE_RXD: // закончился прием данных
        SPIM_TransactionStop(instance);
      break;
    }
  }
}

static bool spiStartWriteRead(void *instance, uint32_t wait_ticks)
{ // запуск операции записи/чтения через DMA
  // все необходимые данные по транзакции лежат в структуре spim_instance_t
  
  spim_instance_t *dev = (spim_instance_t *)instance;
  
  dev->spim->INTENCLR = 0xFFFFFFFF; // запрет всех прерываний
  
  // сбрасываем флаги возможных прерываний
  dev->spim->EVENTS_END = 0; // конец приема и передачи
  dev->spim->EVENTS_STOPPED = 0; // флаг окончания транзакции
  
  // настраиваю пины
  if(dev->q->csPin != SPI_PIN_NOT_USED)
  { // используется CS
    if(dev->q->csPin >= 32)
    {
      NRF_P1->OUTCLR = (1 << (dev->q->csPin - 32)); // CS = 0
      NRF_P1->DIRSET = (1 << (dev->q->csPin - 32)); // CS на выход
    }else{
      NRF_P0->OUTCLR = (1 << dev->q->csPin); // CS = 0
      NRF_P0->DIRSET = (1 << dev->q->csPin); // CS на выход
    }
  }
  
  dev->spim->RXD.PTR = (uint32_t)dev->q->rxData;
  dev->spim->RXD.MAXCNT = 0; // пока ничего не принимаем
  dev->spim->TXD.PTR = (uint32_t)dev->q->txData;
  dev->spim->TXD.MAXCNT = 0; // пока ничего не передаем
  
  if(dev->q->cmdBuffLen != 0)
  { // есть команда
    dev->mode = SPIM_MODE_CMD;
    if((dev->q->cmdBuffLen == 1) && (dev->q->cmdBuff == 0)) dev->spim->TXD.PTR = (uint32_t)&dev->q->cmd;
    else dev->spim->TXD.PTR = (uint32_t)dev->q->cmdBuff;
    dev->spim->TXD.MAXCNT = dev->q->cmdBuffLen;

  }else if(dev->q->txDataLen != 0){ // команды нет; нужно что-то передать
    dev->mode = SPIM_MODE_TXD;
    dev->spim->TXD.MAXCNT = dev->q->txDataLen;
    
  }else{ // нужно только принимать
    dev->mode = SPIM_MODE_RXD;
    dev->spim->RXD.MAXCNT = dev->q->rxDataLen;
  }
  
  xSemaphoreTake(dev->irqSema, 0); // сбрасываю семафор (на всякий случай)
  
  dev->spim->INTENSET = SPIM_INTENSET_END_Msk; // разрешаю прерывание
  dev->spim->TASKS_START = 1; // стартую транзакцию
  
  return xSemaphoreTake(dev->irqSema, wait_ticks); // жду окончания выполнения команды
}


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

uint16_t spiInit(uint8_t *devID, NRF_SPIM_Type *spim, IRQn_Type irqn, uint8_t irqPrior, uint32_t freq)
{
  if(devID == NULL) return ERR_INVALID_PARAMETR;
  
  uint8_t id = 0;
  while(id < SPIM_INSTANCE_CNT)
  {
    if(spim_instance[id].isInited == 0) break;
    id++;
  }
  
  if(id >= SPIM_INSTANCE_CNT) return ERR_NO_SPACE;
  
  *devID = id; // возвращаю номер ID
  
  spim_instance_t *dev = (spim_instance_t *)&spim_instance[id]; // указатель на объект класса интерфейса
  
  memset((void*)dev, 0, sizeof(spim_instance_t));
  
  uint16_t err_code;
  
  err_code = sd_nvic_DisableIRQ(irqn);
  ERROR_CHECK(err_code);
  
  // настройки в соответсвии с интерфейсами
if(spim == NRF_SPIM3){
    
    sysSetSpim3Hook(irqHandler, dev); // обработчик SPIM3
    
    // "подключаем" пины
    spim->PSEL.SCK = SPIM3_SCK_PIN;
    spim->PSEL.MISO = SPIM3_MISO_PIN;
    spim->PSEL.MOSI = SPIM3_MOSI_PIN;
    // пины CS подключается при необходимости
    
    // задаем скорость обмена по шине
    spim->FREQUENCY = freq;
    
    // режим работы и очередность сдвига бит
    spi_configure(spim, SPI_MODE_1, SPI_BIT_ORDER_MSB_FIRST);
    
    // использую списки
    spim->RXD.LIST = 1;
    spim->TXD.LIST = 1;
    
    // шоткаты не использую
    spim->SHORTS = 0;
    
    spim->ORC = 0x00; // этот байт будет передаваться, когда принять нужно больше, чем передать
    
    spim->INTENCLR = 0xFFFFFFFF; // запрещаем все прерывания
    
    // настраиваю пины
    SPIM3_PINS_INIT();
  }else{
    return ERR_DATA;
  }
  
  dev->spim = spim;
  dev->IRQn = irqn;
  dev->irqPrior = irqPrior;
  
  // создаю мьютекс окончания выполнения текущей задачи
  dev->mutex = xSemaphoreCreateMutex();
  if(dev->mutex == NULL) err_code = ERR_OUT_OF_MEMORY;
  ERROR_CHECK(err_code);
  xSemaphoreGive(dev->mutex); // никаких задач не выполняется
  
  // создаю семафор выхода из прерывания
  dev->irqSema = xSemaphoreCreateBinary();
  if(dev->irqSema == NULL) err_code = ERR_OUT_OF_MEMORY;
  ERROR_CHECK(err_code);
  
  // устанавливаю приоритеры прерываний
  err_code = sd_nvic_SetPriority(irqn, irqPrior);
  ERROR_CHECK(err_code);
  
  // разрешаем прерывания
  err_code = sd_nvic_EnableIRQ(irqn);
  ERROR_CHECK(err_code);

  dev->isInited = true;
  spim->ENABLE = SPIM_ENABLE_ENABLE_Enabled; // разрешаю модуль SPIM
  
  return (err_code);
}

uint16_t spiDeInit(uint8_t devID)
{
  if((devID >= SPIM_INSTANCE_CNT)||(!spim_instance[devID].isInited)) return ERR_INVALID_PARAMETR;
  
  spim_instance_t *dev = (spim_instance_t *)&spim_instance[devID];
  
  dev->isInited = false;
  sd_nvic_DisableIRQ(dev->IRQn);
  // "отключаем" пины
  dev->spim->PSEL.SCK = 0;
  dev->spim->PSEL.MISO = 0;
  dev->spim->PSEL.MOSI = 0;
  dev->spim->ENABLE = 0;
  vSemaphoreDelete(dev->mutex);
  vSemaphoreDelete(dev->irqSema);
  return(NRF_SUCCESS);
}

uint16_t spiTaskExecute(uint8_t devID, spi_queue_t *task, uint32_t wait_ticks)
{ // выполнение очередной задачи
  if((devID >= SPIM_INSTANCE_CNT)||(!spim_instance[devID].isInited)) return (ERR_NOT_INITED);
  spim_instance_t *dev = (spim_instance_t *)&spim_instance[devID];
  
  if(pdFALSE == xSemaphoreTake(dev->mutex, wait_ticks))
    return ERR_TIMEOUT; // выход по таймауту ожидания мьтекса
  
  uint16_t err = ERR_NOERROR;
  
  dev->q = task; // текущая задача
  
  // стартую выполнение задачи
  if(pdFALSE == spiStartWriteRead(dev, SPIM_TASK_TIMEOUT_MS)) err = ERR_TIMEOUT;
  
  xSemaphoreGive(dev->mutex);
  return err;
}





