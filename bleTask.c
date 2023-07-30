/*
Реализация логики работы BLE в проекте


***********************************************************************************************************************

СДЕЛАТЬ


ОСОБЕННОСТИ
- при подключении, приемный и передающий буферы очищаю
- максимальная длительность связи с телефоном не ограничена
- через sdk_config.h настроено для работы от внутренного RC (модуль E73 не имеет часового кварца): 
-- NRF_SDH_CLOCK_LF_SRC 0
-- NRF_SDH_CLOCK_LF_RC_CTIV 16
-- NRF_SDH_CLOCK_LF_RC_TEMP_CTIV 2
-- NRF_SDH_CLOCK_LF_ACCURACY 1

ТРЕБОВАНИЯ к структуре эдертайзинг пакета
- в manufacturer data сначала идет байт-идентификатор, по которому определяю стурктуру остальных данных

ОГРАНИЧЕНИЯ
- поддерживается только один канал связи в качестве перефирийного устройства (подключение с телефона), иначе нужно делать отдельную задачу по менеджменту


*/


#include "bleTask.h"
#include "ble_gap.h"
#include "ble_advdata.h"

#include "errors.h"
#include "custom_board.h"
#include "ble_advertising.h"
#include "ble_nus.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"


// НАСТРОЙКИ МОДУЛЯ ************************************
#define RTT_DEBUG_EN                    0     // выводить дебажные сообщения в RTT

#if(RTTLOG_EN)
#include "logger_freertos.h"
#define RTT_LOG_EN                      1     // включить лог через RTT
#else
#define RTT_LOG_EN 0
#endif // RTTLOG_EN
// *****************************************************

#if(RTT_LOG_EN)
#define RTT_LOG_INFO(...)       \
                                \
  {                             \
    NRF_LOG_INFO(__VA_ARGS__);  \
  }
#if(RTT_DEBUG_EN)
#define RTT_LOG_DEBUG(...)       \
                                \
  {                             \
    NRF_LOG_INFO(__VA_ARGS__);  \
  }
#else
  #define RTT_LOG_DEBUG(...) {}
#endif // RTT_LOG_EN
#else
  #define RTT_LOG_INFO(...) {}
  #define RTT_LOG_DEBUG(...) {}
#endif // RTT_LOG_EN

  
  


#ifndef NUS_RX_SIZE_MAX
#define NUS_RX_SIZE_MAX           128    // размер приемного фифо для телефона
#endif
#ifndef NUS_TX_SIZE_MAX
#define NUS_TX_SIZE_MAX           1024    // размер передающего потокового буфера для телефона
#endif
#ifndef UNIT_NUS_EVT_QUEUE_SIZE
#define UNIT_NUS_EVT_QUEUE_SIZE         10      // размер очереди сообщений на верхний уровень
#endif



typedef struct
{
  int16_t      conn_handle;        // хендл подключения (= -1, если подключение не установлено)
  StreamBufferHandle_t nusStreamTx; // фифо NUS для передачи, обслуживающее этот канал связи
  StreamBufferHandle_t nusStreamRx; // фифо NUS для приема, обслуживающее этот канал связи
} conn_table_t;

typedef struct
{ // для передачи информации о пароле в sendMsg()
  char                    psw[10];
} evtPswData_t;


static QueueHandle_t        m_evtQueueHandle; // очередь событий для верхнего уровня
static conn_table_t         m_connTable[NRF_BLE_LINK_COUNT]; // таблица соединений (в качестве индекса используется conn_handle_t)


// ##################################### ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ######################################################

static void sendMsg(bleTaskEvents_t evtID, conn_handle_t conn_handle, void *data)
{ // отправка сообщения на верх
    bleTaskEvtData_t evt = {
      .evtID = evtID,
      .conn_handle = conn_handle
    };
    
    // помещаю сообщение в очередь
    if(m_evtQueueHandle != NULL)
    {
      if(pdPASS != xQueueSendFromISR(m_evtQueueHandle, (const void *)&evt, NULL))
      { // переполнение очередь
        RTT_LOG_INFO("BLE TASK msg queue ovf");
      }
    }
}

// ##################################### ОБРАБОТЧИКИ СОБЫТИЙ НИЖНЕГО УРОВНЯ ######################################################

static void  nusBlePerepheralEvtHandler(bleCallback_t *data)
{ // дополнительный обработчик данных для ble_perepheral_evt_handler (обрабатывает подключения телефона)
  bleCallback_t *p_data = (bleCallback_t *)data;
  
  switch(p_data->evtType)
  {
    case BLE_EVENT_TYPE_CONNECT: // это событие будет вызвано ТОЛЬКО ПОСЛЕ установления защищенного соединения
    {
      RTT_LOG_DEBUG("BLE TASK: BLE_EVENT_TYPE_CONNECT");

      if(p_data->conn_handle == BLE_CONN_HANDLE_INVALID)
      { // в таблице нет свободного места
        RTT_LOG_INFO("BLE TASK: conn_handle invalid");
        break;
      }

      // к нам подключился телефон
      // передаю информацию о буферах для этого подключения в драйвер
      bleSetNusBuffers(p_data->conn_handle, m_connTable[p_data->conn_handle].nusStreamRx, m_connTable[p_data->conn_handle].nusStreamTx);
      // очищаю входной буфер
      bleResetRxBuff(p_data->conn_handle);
      // очищаю выходной буфер
      bleResetTxBuff(p_data->conn_handle);
      sendMsg(BLE_TASK_CONNECTED, p_data->conn_handle, NULL); // отправляю уведомление на верхний уровень
    }
    break;
  
    case BLE_EVENT_TYPE_DISCONNECT: // событие о разрыве соединения
    {
      RTT_LOG_DEBUG("BLE TASK: BLE_EVENT_TYPE_DISCONNECT");
      if(p_data->conn_handle != BLE_CONN_HANDLE_INVALID) m_connTable[p_data->conn_handle].conn_handle = BLE_CONN_HANDLE_INVALID;
      sendMsg(BLE_TASK_DISCONNECTED, p_data->conn_handle, NULL); // отправляю уведомление
    }
    break;
    
    case BLE_EVENT_TYPE_NUS_RX: // приняты данные по каналу NUS
    {
      RTT_LOG_DEBUG("BLE TASK: BLE_EVENT_TYPE_NUS_RX");
      if(p_data->conn_handle == BLE_CONN_HANDLE_INVALID) break;
      sendMsg(BLE_TASK_RX, p_data->conn_handle, NULL); // отправляю сообщение на верхний уровень
    }
    break;
    
    case BLE_EVENT_TYPE_GAP: // прочие эвенты от GAP
      RTT_LOG_DEBUG("BLE TASK: BLE_EVENT_TYPE_GAP");
      switch(p_data->evt.gapEvt)
      {
        case BLE_GAP_EVT_ADV_SET_TERMINATED: // эдвертайзинг закончился по таймауту
          RTT_LOG_DEBUG("BLE TASK: BLE_GAP_EVT_ADV_SET_TERMINATED");
          bleAdvStart(NULL, 0, 0, BLE_PWR_0, 0, 0, BLE_PWR_0, NULL, 0, false); // пробую перезапустить со старыми параметрами
        break;
        
        case BLE_GAP_EVT_CONNECTED: // случилось подключение (авторизация еще не прошла)
        {
          RTT_LOG_DEBUG("BLE TASK: BLE_GAP_EVT_CONNECTED");
          // добавляю устройство в таблицу
          if(p_data->conn_handle != BLE_CONN_HANDLE_INVALID)
          {
            m_connTable[p_data->conn_handle].conn_handle = p_data->conn_handle;
          }
        }
        break;
        
        default:
        break;
      }
    break;
      
    case BLE_EVENT_TYPE_PASSWORD: // получили KEY (передается как указатель на строку), который надо ввести для паринга
      RTT_LOG_DEBUG("BLE TASK: BLE_EVENT_TYPE_PASSWORD");
      // если требуется подтверждение, то выполняю его "не глядя"
      if(p_data->evt.pswReq.match_request) 
      {
        bleAuthKeyReply(p_data->conn_handle);
      }else{
        // пароль который нужно ввести на экране телефона
        RTT_LOG_INFO("BLE: PERIPH: Password %s request", nrf_log_push(p_data->evt.pswReq.spasskey));
      }
    break;
      
    default:
      break;
  }
}

static void bleEventHandler(bleCallback_t *p_args)
{  // обработчик сообщения от драйвера ble
  
  switch(p_args->evtSrc)
  {
    case BLE_EVENT_SRC_PERIPHERAL:
      nusBlePerepheralEvtHandler(p_args);
    break;
    
    default:
    break;
  } // switch
}


// ##################################### ЗАДАЧИ УПРАВЛЕНИЯ ######################################################



// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// ############################################## ВНЕШНИЕ ФУНКЦИИ ###############################################

ret_code_t bleTaskInit(void)
{ // инициализация задачи
  
  // инициализирую данные для эдвертайзинга
//  nusInitManufData((uint8_t*)&m_manuf_data, sizeof(m_manuf_data));
  
  // настраиваю блютус: для сопряжения будет использоваться дефолтрый пароль
  ERROR_CHECK(bleInit(bleEventHandler, true));
  
  // настраиваю сервис DIS
  ERROR_CHECK(bleDeviceInfoInit(MANUFACTURER_NAME, FW_VER, HW_VER, MANUFACTURER_ID, ORG_UNIQUE_ID));
  
  // инициализирую очередь сообщений для верхнего уровня
  m_evtQueueHandle = xQueueCreate(UNIT_NUS_EVT_QUEUE_SIZE, sizeof(bleTaskEvtData_t));
  if(m_evtQueueHandle == NULL) return NRF_ERROR_NO_MEM;
  
  memset(&m_connTable, 0, sizeof(m_connTable)); // очищаю таблицу поддерживаемых устройств
  for(uint8_t i = 0; i < NRF_BLE_LINK_COUNT; i++)
  {
    m_connTable[i].conn_handle = BLE_CONN_HANDLE_INVALID;
    m_connTable[i].nusStreamRx = xStreamBufferCreate(NUS_RX_SIZE_MAX, 1); // создаю потоковый буфер с триггером в 1 байт
    if(m_connTable[i].nusStreamRx == NULL) return NRF_ERROR_NO_MEM;
    m_connTable[i].nusStreamTx = xStreamBufferCreate(NUS_TX_SIZE_MAX, 1); // создаю потоковый буфер с триггером в 1 байт
    if(m_connTable[i].nusStreamTx == NULL) return NRF_ERROR_NO_MEM;
  }
  
#if(BLE_NO_ADV == 0)
  // запускаю эдвертайзинг (данные производителя не используются, подключения разрешены)
  vTaskDelay(pdMS_TO_TICKS(10)); // задержка нужна для запуска задачи из bleInit()
  ERROR_CHECK(bleAdvStart(DEVICE_NAME, TIME_ADV_INTERVAL_MS, TIME_ADV_DURATION_MS, BLE_ADV_POWER_MAX, 
              TIME_CONN_INTERVAL_MIN_MS, TIME_CONN_INTERVAL_MAX_MS, BLE_CONN_POWER_MAX, NULL, 0, true));
#else
  vTaskDelay(pdMS_TO_TICKS(10)); // задержка нужна для запуска задачи из bleInit()
#endif // BLE_NO_ADV

  return NRF_SUCCESS;
}


bool bleTaskWaitEvt(bleTaskEvtData_t *evt, uint32_t wait_ms)
{ // получение сообщения из очереди
  // возвращает прочитанные данные, если данные прочитаны
  // возвращает false, если вышли по таймауту
  if(m_evtQueueHandle == NULL) return false;
  if(evt == NULL) return false;
  return (pdTRUE == xQueueReceive(m_evtQueueHandle, (void *)evt, pdMS_TO_TICKS(wait_ms)));
}


/*
* Очищает очередь сообщений
*/
void bleTaskEvtClear(void)
{
  if(m_evtQueueHandle == NULL) return;
  xQueueReset(m_evtQueueHandle);
}


bool bleTaskTxData(conn_handle_t conn_handle, uint8_t *buff, uint16_t size, SemaphoreHandle_t sema)
{ // передача бинарных данных, данные собираются в пакет
  // соединение с приемником данных должно быть уже установлено, иначе данные будут потеряны
  // возвращает false, если не удалось добавить в буфер

  
  
  return false;
}


/*
* Передача произвольных данных через NUS с ожиданием, пока освободится место в буфере
* conn_handle - хендл устройства, которому нужно передать данные
* buff - указатель на буфер с данными
* size - размер данных в буфере
* wait_ms - максимальное время ожидания доступного места в буфере
* Возвращает false, если данные по каким-либо причинам (переполение буфера, ни одного сборщика не подключено) не могут быть переданы
*/
bool bleTaskTxDataWait(conn_handle_t conn_handle, uint8_t *buff, uint16_t size, uint32_t wait_ms)
{
  if(buff == NULL) return false;
  if(conn_handle >= NRF_BLE_LINK_COUNT) return false;
  
  if(NRF_SUCCESS != bleNusTxWait(m_connTable[conn_handle].conn_handle, buff, size, wait_ms)) return false;
  return true;
}


/*
* Запрос количества данных в приемном буфере
* возвращает количество данных
*/
uint32_t bleGetRxDataSize(const conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return 0;
  
  if(m_connTable[conn_handle].nusStreamRx == NULL) return 0;
  
  return xStreamBufferBytesAvailable(m_connTable[conn_handle].nusStreamRx);
}


/*
* Запрос количества свободного места в приемном буфере
* возвращает количество данных
*/
uint32_t bleGetRxFreeSize(const conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return 0;
  
  if(m_connTable[conn_handle].nusStreamRx == NULL) return 0;
  
  return xStreamBufferSpacesAvailable(m_connTable[conn_handle].nusStreamRx);
}


/*
* Запрос свободного места в передающем буфере
* возвращает размер свободного места
*/
uint32_t bleGetTxFreeSpace(const conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return 0;
  
  if(m_connTable[conn_handle].nusStreamTx == NULL) return 0;
  
  return xStreamBufferBytesAvailable(m_connTable[conn_handle].nusStreamTx);
}


/*
* Очистка приемного буфера
*/
void bleResetRxBuff(const conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return;
  
  if(m_connTable[conn_handle].nusStreamRx == NULL) return;
  
  xStreamBufferReset(m_connTable[conn_handle].nusStreamRx);
}


/*
* Очистка передающего буфера
*/
void bleResetTxBuff(const conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return;
  
  if(m_connTable[conn_handle].nusStreamTx == NULL) return;
  
  xStreamBufferReset(m_connTable[conn_handle].nusStreamTx);
}

/*
* Вычитывание заданного количества данных из входного буфера
* conn_handle - ID устройства
* buff - буфер для размещения данных
* size - количество данных для чтения
* wait_ms - время ожидания данных в буфере
* возвращает количество вычитанных данных
*/
uint32_t bleGetRxData(const conn_handle_t conn_handle, void *buff, uint32_t size, uint32_t wait_ms)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return 0;
  if(m_connTable[conn_handle].nusStreamRx == NULL) return 0;
  
  return xStreamBufferReceive(m_connTable[conn_handle].nusStreamRx, buff, size, pdMS_TO_TICKS(wait_ms));
}


/*
* Отключение устройства
* conn_handle - ID устройства
*/
void bleDisconnectUnit(conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return;
  
  bleDisconnect(m_connTable[conn_handle].conn_handle);
}


/*
* Запрос хендла соединения устройства
* conn_handle - ID устройства
* возвращает conn_handle
*/
uint16_t bleGetConnHandle(conn_handle_t conn_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return BLE_CONN_HANDLE_INVALID;
  return m_connTable[conn_handle].conn_handle;
}
