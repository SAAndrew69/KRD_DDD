
/**
 *  Реализация логики работы устройства
 * 
 *  ОГРАНИЧЕНИЯ
 * - если было приянято несколько команд управления, то будет обработана только одна (самая первая), все остальное будет удалено
*/

#include "settings.h"
#include "custom_board.h"
#include "sys.h"
//#include "spim_freertos.h"
#include "ads_task.h"
#include "ads1298.h"  // берем только константу ADS1298_REG_LAST
#include "bleTask.h"
#include "cmd.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "nrf_drv_power.h"


/* FreeRTOS related */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#include "math.h"
#include "errors.h"


// НАСТРОЙКИ МОДУЛЯ ************************************
#define MAIN_BLE_ACD_START_MARKER   0xFFFF  // маркер пакета данных от АЦП

// программирую напряжение питания GPIO в 3.3V (по адресу 0x10001304 будет записано значение UICR_REGOUT0_VOUT_3V3)
const uint32_t UICR_REGOUT0 __attribute__((at(0x10001304))) __attribute__((used)) = UICR_REGOUT0_VOUT_3V3; 

#define TIME_WAIT_SUPERTASK_ACCESS    3000  // максимальное время ожидания доступа к очереди сообщения для суперзадачи (вместо portMAX_DELAY)
#define TIME_CMD_MS                   500   // таймаут выполнения команд 


#if(RTTLOG_EN)
#include "logger_freertos.h"
#define RTT_LOG_EN                1     // включить лог через RTT
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
#else
  #define RTT_LOG_INFO(...) {}
#endif // RTT_LOG_EN
  
  
  
#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#endif
  
  
typedef enum
{ // идентификаторы сообщений для суперзадачи
  SUPER_MSG_NONE =0,
  SUPER_MSG_RX_DATA,          // приняты данные
  SUPER_MSG_REBOOT,           // перезагрузка устройства
  SUPER_MSG_BLE_CONNECTED,    // подключение по BLE
  SUPER_MSG_BLE_DISCONNECTED, // отключение по BLE
} superMsg_id_e;
  
  
typedef struct
{ // структура сообщения для суперзадачи
  superMsg_id_e       msgID;      // ID сообщения
  uint16_t            msgLen;     // длина сообщения
  uint8_t             msg[4];     // размер сообщения взят с "потокла", при необходимости изменить)
} superMsg_t;

__packed typedef struct
{ // формат данных от АЦП для передачи по BLE
  uint16_t startMarker;       // признак начала посылки
  int16_t adc0[ADS129X_CH_CNT]; // каналы АЦП 0
  int16_t adc1[ADS129X_CH_CNT]; // каналы АЦП 1
} adc_data_format_t;
  
  
static TaskHandle_t           m_superTask = NULL; // хендл суперзадачи для реализации всей логики работы  
static QueueHandle_t          m_superMsgHandle = NULL; // хендл буфера сообщений для суперзадачи 
static uint8_t                m_cmdBuff[CMD_LEN_MAX]; // буфер для принятой команды
static adc_data_format_t      m_adcData; // буфер для формирования послыки от АЦП в канал BLE
static conn_handle_t          m_conn_handle = NULL; // хендл канала связи BLE
static bool                   m_adc_started = false; // флаг запущенного АЦП
static uint32_t               m_adc_sample_cnt = 0; // счетчик сэмплов АЦП TEST

// TEST
#define TEST_ARR_SIZE     1024
static TaskHandle_t       m_testTask = NULL;
static uint8_t test_array1[TEST_ARR_SIZE]; // для тестирования скорости передачи
static uint8_t test_array2[TEST_ARR_SIZE]; // для тестирования скорости передачи

// #############################  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ  ##############################################
#if ADS129X_EN
static void ads_task_callback(adstask_data_t *ads_data)
{ // в эту функцию прилетают данные от двух АЦП в формате adstask_data_t
//  if(m_adc_sample_cnt == 3) 
//  { // TEST
//    NRF_LOG_INFO("ads_data:");
//    NRF_LOG_HEXDUMP_INFO(ads_data, sizeof(adstask_data_t));
//  }
  // формирую пакет для отправки
  for(uint8_t i=0; i < ADS129X_CH_CNT; i++)
  { // преобразую 24 бит в 16
    m_adcData.adc0[i] = (int16_t)(ads_data->adc0[i] >> 8);
    m_adcData.adc1[i] = (int16_t)(ads_data->adc1[i] >> 8);
  }
  m_adcData.startMarker = MAIN_BLE_ACD_START_MARKER;

  if(m_conn_handle >= 0)
  { // соединение все еще установлено
    if(!bleTaskTxDataWait(m_conn_handle, (uint8_t *)&m_adcData, sizeof(m_adcData), BLE_SEND_TIMEOUT_MS))
    { // передающий буфер переполнен, пакет данных будет потерян
      RTT_LOG_INFO("MAIN: BLE tx queue ovf, adc data lost");
    }
    
//    if(m_adc_sample_cnt == 3) 
//    { // TEST
//      NRF_LOG_INFO("m_adcData:");
//      NRF_LOG_HEXDUMP_INFO(&m_adcData, sizeof(m_adcData));
//    }
    
    m_adc_sample_cnt++;
  }
}
#endif // ADS129X_EN

static bool sendSuperMsgFunc(superMsg_id_e msgID, void *msgData, uint16_t msgSize, uint32_t timeout_ms)
{ // отправка сообщения в суперзадачу
  // msgID - ID сообщения
  // msgData - указатель на данные (может быть NULL)
  // msgSize - размер структуры сообщения (может быть = 0)
  // timeout_ms - таймаут ожидания помещения сообщения в буфер, если буфер заполнен
  // возвращает TRUE, если сообщение успешно помещено в буфер
  
  superMsg_t msg;
  memset(&msg, 0, sizeof(msg));
  msg.msgID = msgID;
  if((msgData != NULL)&&(msgSize != 0))
  {
    if(msgSize > sizeof(msg.msg)) msgSize = sizeof(msg.msg);
    memcpy(&msg.msg, msgData, msgSize);
  }
  
  if(0 == xQueueSend(m_superMsgHandle, (const void *)&msg, pdMS_TO_TICKS(timeout_ms))) return false;
  return true;
}

static bool sendSuperMsg(superMsg_id_e msg_id)
{ // отправка сообщения в суперзадачу БЕЗ данных
 
  return sendSuperMsgFunc(msg_id, NULL, 0, TIME_WAIT_SUPERTASK_ACCESS);
}

static bool sendSuperMsgSrc(superMsg_id_e msg_id, uint8_t src)
{ // отправка сообщения в суперзадачу с источником команды в данных
  return sendSuperMsgFunc(msg_id, &src, sizeof(src), TIME_WAIT_SUPERTASK_ACCESS);
}


// #############################  BLE  ##################################################################
#if BLE_EN
static uint16_t ble_advRestart(void)
{
#if(BLE_NO_ADV == 0)            
  // ПЕРЕЗАПУСКАЮ ЭДВЕРТАЙЗИНГ в зависимости от флага GF_EXT_PWR_PRESENT
  uint32_t repCnt = 0;
  do{ // делаю несколько попыток запустить эдвертайзинг в случае неуспеха
    uint16_t err;
    RTT_LOG_INFO("MAIN: Restart advertising ...");
    err = bleAdvStart(DEVICE_NAME, TIME_ADV_INTERVAL_MS, TIME_ADV_DURATION_MS, BLE_ADV_POWER_MAX, 
            TIME_CONN_INTERVAL_MIN_MS, TIME_CONN_INTERVAL_MAX_MS, BLE_CONN_POWER_MAX, NULL, 0, true);
    if(err == NRF_SUCCESS) break;
    
    RTT_LOG_INFO("MAIN: Restart advertising error %d", err);
    repCnt++;
    if(repCnt >= BLE_ADV_ERROR_MAX)
    { // эдвертайзинг не запускается
      return err;
    }
    vTaskDelay(pdMS_TO_TICKS(BLE_ADV_START_DELAY_MS));
  }while(1);
  
  RTT_LOG_INFO("MAIN: Advertising restarted");
#endif // BLE_NO_ADV
  return NRF_SUCCESS;
}

static void print_ble_fds_stats(void)
{
  ble_fds_stat_t stats;
  
  if(NRF_SUCCESS != blePrintFlashStats(&stats)) return;
  
  // вывожу полученную статистику в консоль
  RTT_LOG_INFO("pages_available = %d", stats.pages_available);
  RTT_LOG_INFO("open_records = %d", stats.open_records);
  RTT_LOG_INFO("valid_records = %d", stats.valid_records);
  RTT_LOG_INFO("dirty_records = %d", stats.dirty_records);
  RTT_LOG_INFO("words_used = %d", stats.words_used);
  RTT_LOG_INFO("largest_contig = %d", stats.largest_contig);
  RTT_LOG_INFO("freeable_words = %d", stats.freeable_words);
  RTT_LOG_INFO("if corruption? = %d", stats.corruption);
  
  if(stats.largest_contig < 950) bleGarbageCollector();
}

static void ble_thread(void *args)
{ // процесс приема сообщений от драйвера BLE
  
  bleTaskEvtData_t evt;

  uint16_t err = bleTaskInit();
  if(err != ERR_NOERROR)
  {
    RTT_LOG_INFO("BLE_THREAD: Ble Task init error: 0x%04X", err);
    // TODO добавить индикацию
    vTaskDelete(NULL);
    return;
  }

  bleParingEn(true); // паринг разрешен
  
  for(;;)
  {
    // жду появления данных
    bleTaskWaitEvt(&evt, portMAX_DELAY);

    switch(evt.evtID)
    {
      case BLE_TASK_CONNECTED: // подключение установлено (телефон или любое другое устройство)
        m_conn_handle = evt.conn_handle;
        sendSuperMsg(SUPER_MSG_BLE_CONNECTED);
      break;
      
      case BLE_TASK_DISCONNECTED:
        // произошло отключение
        sendSuperMsg(SUPER_MSG_BLE_DISCONNECTED);

        m_conn_handle = -1;        
        if(ble_advRestart() != NRF_SUCCESS)
        {
          RTT_LOG_INFO("BLE_THREAD: Start advertising error! Rebooting...");
          sendSuperMsg(SUPER_MSG_REBOOT);
          break;
        }

        // TEST вывожу статистику по FDS в консоль
        //print_ble_fds_stats();
      break;
      
      case BLE_TASK_RX: // приняты какие-то данные 
        sendSuperMsg(SUPER_MSG_RX_DATA);
      break;
      
      case BLE_TASK_TX: // очередная порция данных передана
        // НЕ РЕАЛИЗОВАНО НА УРОВНЕ ДРАЙВЕРА
      break;
      
      default:
        break;
    } // switch
  } // for
}

#endif // BLE_EN

// #############################  CMD  ################################################################
void execCmdBle(int16_t conn_handle)
{ // парсер команд управления по каналу BLE
  // команда лежит в приемном буфере BLE: первый байт - код комадны, далее могут идти дополнительные данные
  // будет обработана только первая команда, остальное - в утиль
  uint32_t cnt = bleGetRxDataSize(conn_handle); // размер данных в буфере
  if(cnt == 0) return;
  uint32_t cmdLen = MIN(CMD_LEN_MAX, cnt);
  bleGetRxData(conn_handle, m_cmdBuff, cmdLen, 0); // вычитываю все
  if(cnt >= CMD_LEN_MAX)
  { // если данных буфере больше максимальной длины команды - удаляю все
    bleResetRxBuff(conn_handle); // очищаю буфер принятых сообщений
  }

  switch((char)m_cmdBuff[0]) // код команды
  {
    case CMD_CMD_STATUS : // Выдача идентификационной информации об устройстве
    {
      char buff[50];
      snprintf(buff, sizeof(buff), "Multichannel cardiograph\n");
      bleTaskTxDataWait(m_conn_handle, (uint8_t *)buff, strlen(buff), BLE_SEND_TIMEOUT_MS);
      snprintf(buff, sizeof(buff), "Firmware:%s\n", FW_VER);
      bleTaskTxDataWait(m_conn_handle, (uint8_t *)buff, strlen(buff), BLE_SEND_TIMEOUT_MS);
      uint64_t uid = GET_DEVICE_ID();
      snprintf(buff, sizeof(buff), "SerialNO:ECL-Cardio18.v%s-%08X%08X\n", HW_VER, (uint32_t)(uid >> 32), (uint32_t)uid);
      bleTaskTxDataWait(m_conn_handle, (uint8_t *)buff, strlen(buff), BLE_SEND_TIMEOUT_MS);
      snprintf(buff, sizeof(buff), "battery_status:OK 100\n"); // TODO подставить реальное значение
      bleTaskTxDataWait(m_conn_handle, (uint8_t *)buff, strlen(buff), BLE_SEND_TIMEOUT_MS);
      snprintf(buff, sizeof(buff), "Custom made for EC-Leasing\n");
      bleTaskTxDataWait(m_conn_handle, (uint8_t *)buff, strlen(buff), BLE_SEND_TIMEOUT_MS);
    }
    break;

    case CMD_CMD_START  : // Запуск процесса измерения с указанием времени съема
      if(m_adc_started) break; // на выход, АЦП уже запущено
      if(ERR_NOERROR == ads_task_start(false))
      {
        m_adc_started = true;
        m_adc_sample_cnt = 0;
      }else{
        RTT_LOG_INFO("CMD: ADC start fail");
      }
    break;

    case CMD_CMD_STOP   : // Останов процесса измерения
      if(ERR_NOERROR == ads_task_stop())
      {
        m_adc_started = false;
        RTT_LOG_INFO("CMD: ADC sample was %d", m_adc_sample_cnt); // TEST
      }else{
        RTT_LOG_INFO("CMD: ADC stop fail");
      }
    break;

    case CMD_CMD_IND    : // Включение/выключение индикации
    break;

    case CMD_CMD_REBOOT : // Перезагрузка устройства
      sendSuperMsg(SUPER_MSG_REBOOT);
    break;

    case CMD_CMD_MAC    : // Выдача МАС-адреса устройства (текстом в шестнадцатиричном виде)
    {
      char buff[30];
      uint64_t uid = GET_DEVICE_ID();
      snprintf(buff, sizeof(buff), "MAC:%08X%08X\n", (uint32_t)(uid >> 32), (uint32_t)uid);
      bleTaskTxDataWait(m_conn_handle, (uint8_t *)buff, strlen(buff), BLE_SEND_TIMEOUT_MS);
    }
    break;

    case CMD_CMD_TIME   : // Запрос текущего времени
    break;
    case CMD_CMD_PDOWN  : // Принудительное выключение устройства
    break;
    case CMD_CMD_CONSUM : // Выдать текущее потребление устройства
    break;
    case CMD_CMD_IND_ON : // Зажечь светодиод индикации состояния
    break;
    case CMD_CMD_BNT    : // Выдать текущее состояние кнопки включения
    break;
    case CMD_CMD_DELTA  : // Сдвинуть время на дельту
    break;
    case CMD_CMD_LOG    : // Выдать лог работы
    break;
    case CMD_CMD_GAUGE  : // Выдать информацию по Gas gauge
    break;
    case CMD_CMD_BAT_ON : // Зажечь сегмент индикатора уровня заряда
    break;
    case CMD_CMD_VIBRO  : // Инициировать виброиндикатор
    break;
    case CMD_CMD_GET_CAL: // Считать калибровочный коэффициент по каналу
    break;
    case CMD_CMD_SET_CAL: // Записать калибровочный коффициент по каналу
    break;
    
    case CMD_CMD_GET_CFG: // Запрос конфига (формат: G,n где n - номер АЦП: 0 или 1)
    {
        uint8_t adc_no = m_cmdBuff[2] - '0'; // преобразую номер АЦП в число
        if(adc_no >= ADS129X_CNT)
        {
          RTT_LOG_INFO("CMD: Wrong ADC number: it can be either 0 or 1");
          break;
        }
        char str[150];
        memset(str, 0, sizeof(str));
        snprintf(str, sizeof(str), "%c,", CMD_CMD_GET_CFG);
        uint16_t err = ads_task_get_config((adstask_adc_no_e)adc_no, (char *)&str[2], sizeof(str)-2, TIME_CMD_MS);
        if(err != ERR_NOERROR)
        {
          RTT_LOG_INFO("CMD: ADC read regs error 0x%04X", err);
          break;
        }
        // в str сформирован ответ вида: G,n,rrvv,....,rrvv где n - номер АЦП (0 или 1), rrvv - uint16_t, где rr - адрес регистра, vv - значение регистра
        bleTaskTxDataWait(m_conn_handle, (uint8_t *)str, strlen(str), BLE_SEND_TIMEOUT_MS);
    }
    break;
    
    case CMD_CMD_SET_CFG: // Установка нового конфига (формат: S,n,rrvv,...,rrvv где n - номер АЦП, rrvv - uint16, где rr - адрес регистра, vv - значение)
    {
      uint8_t adc_no = m_cmdBuff[2] - '0'; // преобразую номер АЦП в число
      if(adc_no >= ADS129X_CNT)
      {
        RTT_LOG_INFO("CMD: Wrong ADC number: it can be either 0 or 1");
        break;
      }
      // начиная с позиции 4 идут uint16 разделенные запятой
      char *pVal = strtok((char *)&m_cmdBuff[4],",");
      while(pVal)
      { // запись принятых значений в АЦП
        uint16_t val = strtol(pVal, NULL, 16); // выделяю значение очередной пары
        if(val == 0) 
        {
          RTT_LOG_INFO("CMD: Recieve invalid pair");
          break; // принято недопустимое значение
        }
        uint8_t regAddr = val >> 8; // значение адреса регистра
        uint8_t regVal = (uint8_t)val; // значение регистра
        // отправляю новое значение в АЦП
        uint16_t err = ads_task_set_reg((adstask_adc_no_e)adc_no, regAddr, regVal, TIME_CMD_MS);
        if(err != ERR_NOERROR)
        {
          RTT_LOG_INFO("CMD: Set new val 0x%02X to addr 0x%02X error 0x%04X", regVal, regAddr, err);
        }
        // переходим к следующей паре значений
        pVal = strtok(NULL, ",");
      }
    }
    break;

    case CMD_CMD_SHOT   : // Единичный отсчет АЦП
      if(ERR_NOERROR != ads_task_start(true))
      {
        RTT_LOG_INFO("CMD: ADC start single shot fail");
      }
    break;

    case CMD_CMD_RSSI   : // Выдать уровнь сигнала BLE
    break;
    case CMD_CMD_FW     : // Перейти в режим обновления прошивки
    break;

    default:
    break;
  }

}

// ############################################  TESTS  ###############################################
static void nusSpeedTest(void *args)
{ // тестирование скорости передачи через NUS

  RTT_LOG_INFO("TEST: TASK STARTED");

  for(;;)
  {
    ulTaskNotifyTake(true, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(1000));
    
    uint8_t index = '0';
    uint32_t time_ticks_start;
  
    bool start = false;
    uint32_t cnt = 10240; // счетчик переданных массивов (должно делиться на 2)

    RTT_LOG_INFO("TEST: START tx %d bytes", (cnt * TEST_ARR_SIZE));
    
    while(1)
    {
      // заполняю массив 1
      for(uint16_t i = 0; i < sizeof(test_array1); i++)
      {
        test_array1[i] = index++;
        if(index == 'z') index = '0';
      }
    
      if(!start)
      { // начинаю замер вермени
        time_ticks_start = xTaskGetTickCount();
        start = true;
      }
    
      // закидываю массив 1 в буфер для передачи
      if(!bleTaskTxDataWait(0, (uint8_t *)test_array1, sizeof(test_array1), 10000))
      {
        RTT_LOG_INFO("TEST: Tx test data error!");
        break;
      }

      // заполняю массив 2
      for(uint16_t i = 0; i < sizeof(test_array2); i++)
      {
        test_array2[i] = index++;
        if(index == 'z') index = '0';
      }

      // закидываю массив 1 в буфер для передачи
      if(!bleTaskTxDataWait(0, (uint8_t *)test_array2, sizeof(test_array2), 10000))
      {
        RTT_LOG_INFO("TEST: Tx test data error!");
        break;
      }

      cnt -= 2;
      if(cnt == 0)
      {
        uint32_t time = xTaskGetTickCount() - time_ticks_start; // время передачи в мс (если configTICK_RATE_HZ = 1000)
        RTT_LOG_INFO("TEST: END. Tx time = %d ms", time);
        break;
      }
    }
  }
}

// #############################  SUPERTASK  ############################################################
static void super_task_thread(void *args)
{
  /*
  Суперзадача, в которой реализуется вся логика работы устройства
  */
  typedef enum
  { // рабочий кейс суперзадачи
    STATE_NONE,
    STATE_STARTUP,    // начальная инициализация интерфейсов, запуск задач
    STATE_MAIN,       // основной режим работы
  } state_t;
  
  
  state_t       state = STATE_STARTUP;
#if(WDT_EN)
  xTimerHandle  wdt_timer = NULL;
#endif // WDT_EN
  
  for(;;)
  {
    switch(state)
    {
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
      case STATE_STARTUP: // НАЧАЛЬНЫЙ ЗАПУСК ПРОЦЕССОВ после перезагрузки или включения питания
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
      {
        
#if(WDT_EN)
        // настройка WDT и запуск
        WDT_Run(WDT_TIME_CYCLE_MS); // таймер будет срабатывать каждые WDT_TIME_CYCLE_MS миллисекунды
        WDT_Reset();
        wdt_timer = xTimerCreate("WDTIMER", pdMS_TO_TICKS(WDT_TIME_CYCLE_MS - 5000), true, NULL, wdt_timer_timeout);     
        xTimerStart(wdt_timer, portMAX_DELAY);
#endif // WDT_EN
        
        // читаю и сбрасываю все флаги причины сброса
        uint32_t reg_reser_reason = NRF_POWER->RESETREAS;
        NRF_POWER->RESETREAS = 0xFFFFFFFF; // после включения Softdevice этой командой флаги уже не сбросить - буде ошибка SOFTDEVICE: INVALID MEMORY ACCESS
        
        // инициализирую буфер сообщений
        m_superMsgHandle = xQueueCreate(SUPERTASK_MSG_BUFF_LEN, sizeof(superMsg_t));
        if(m_superMsgHandle == NULL)
        {
          RTT_LOG_INFO("SUPER: Memory not enough for supertask queue");
          state = STATE_NONE;
          break;
        }
        
   
        // НАСТРОЙКА ПЕРЕФИРИИ И ЗАПУСК ПРОЦЕССОВ

#if BLE_EN
        // запускаю задачу обработки данных от BLE
        if (pdPASS != xTaskCreate(ble_thread, "BLE", BLE_TASK_STACK, NULL, BLE_TASK_PRIORITY, NULL))
        {
          RTT_LOG_INFO("SUPER: Can't create ble_thread");
        }
#endif // BLE_EN


#if ADS129X_EN
        // устанавливаю приоритет прерываний от RDY АЦП и разрешаю прерывания от GPIOTE
        NVIC_SetPriority(GPIOTE_IRQn, GPIOTE_IRQ_PRIORITY);
        NVIC_EnableIRQ(GPIOTE_IRQn);

        uint16_t err = ads_task_init(ads_task_callback);
        if(err != ERR_NOERROR) 
        {
          RTT_LOG_INFO("SUPER: Init ADS TASK error 0x%02X", err);
          state = STATE_NONE;
          break;
        }
#endif // ADS129X_EN

        // запускаю задачу тестирования скорости передачи
        if (pdPASS != xTaskCreate(nusSpeedTest, "TEST", 256, NULL, 2, &m_testTask))
        {
          RTT_LOG_INFO("SUPER: Can't create testTask");
        }
        
        RTT_LOG_INFO("SUPER: ******* Startup complete ********");
        
        state = STATE_MAIN;
      }
      break;
// STATE_STARTUP >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      case STATE_MAIN: // ***** РЕАЛИЗАЦИЯ ФУНКЦИОНАЛА УСТРОЙСТВА *****
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      {
        superMsg_t msg; // буфер для очередного сообщения
        // жду очередное сообщение
        xQueueReceive(m_superMsgHandle, (void *)&msg, portMAX_DELAY);
        
        switch(msg.msgID)
        {
// *********************************************************************************************
          case SUPER_MSG_RX_DATA: // приняты данные
            execCmdBle(m_conn_handle);
          break;
// *********************************************************************************************
          case SUPER_MSG_REBOOT: // требуется перезагрузка устройства
            // TODO выполнить все необходимые действия перед перезагрузкой
            RTT_LOG_INFO("SUPER_MSG_REBOOT");
            vTaskDelay(pdMS_TO_TICKS(1000));
            systemReset();
          break;
// *********************************************************************************************
          case SUPER_MSG_BLE_CONNECTED:
            RTT_LOG_INFO("SUPER_MSG_BLE_CONNECTED");
            //if(m_testTask) xTaskNotifyGive(m_testTask);
          break;
// *********************************************************************************************
          case SUPER_MSG_BLE_DISCONNECTED:
            RTT_LOG_INFO("SUPER_MSG_BLE_DISCONNECTED");

            // соединене по BLE разорвано
            if(m_adc_started)
            { // если АЦП запущено, то останавливаю его
              uint16_t err = ads_task_stop();
              if(err != ERR_NOERROR)
              {
                RTT_LOG_INFO("SUPER: ADS Stop error: 0x%04X", err);
              }else{
                // команда помещена в очередь без ошибок
                m_adc_started = false;
              }
            }
          break;
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************

          default:
            RTT_LOG_INFO("SUPER: UNKNOWN MSG");
            break;
        }
      }
      break;
// STATE_MAIN >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      
      default:
        RTT_LOG_INFO(">>>>>>> SUPERTASK ERROR! REBOOTING... <<<<<<<");
        vTaskDelay(pdMS_TO_TICKS(5000));
        systemReset();
        break;
    }
  }
}

// ######################################################################################################




#if(configCHECK_FOR_STACK_OVERFLOW == 1)
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{ // эта функция будет вызываться при переполнении стека какой-либо задачи
  RTT_LOG_INFO("STACK OVF in task %s", pcTaskName);
}
#endif

#if(configUSE_MALLOC_FAILED_HOOK == 1)
void vApplicationMallocFailedHook(void)
{ // эта функция будет вызываться при ошибке выделении памяти
  RTT_LOG_INFO("MALLOC FILED!");
}
#endif

// ОБРАБОТЧИК КРИТИЧЕСКИХ ОШИБОК (задается в функции sd_softdevice_enable())
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    __disable_irq();
#if(RTTLOG_EN)
    NRF_LOG_FINAL_FLUSH();
#endif // RTTLOG_EN

#ifndef DEBUG
    RTT_LOG_INFO("Fatal error");
#else
    switch (id)
    {
#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
        case NRF_FAULT_ID_SD_ASSERT:
            RTT_LOG_INFO("SOFTDEVICE: ASSERTION FAILED");
            break;
        case NRF_FAULT_ID_APP_MEMACC:
            RTT_LOG_INFO("SOFTDEVICE: INVALID MEMORY ACCESS");
            break;
#endif
        case NRF_FAULT_ID_SDK_ASSERT:
        {
            assert_info_t * p_info = (assert_info_t *)info;
            UNUSED_VARIABLE(p_info);
            RTT_LOG_INFO("ASSERTION FAILED at %s:%u",
                          p_info->p_file_name,
                          p_info->line_num);
            break;
        }
        case NRF_FAULT_ID_SDK_ERROR:
        {
            error_info_t * p_info = (error_info_t *)info;
            UNUSED_VARIABLE(p_info);
            RTT_LOG_INFO("ERROR %u [%s] at %s:%u\r\nPC at: 0x%08x",
                          p_info->err_code,
                          nrf_strerror_get(p_info->err_code),
                          p_info->p_file_name,
                          p_info->line_num,
                          pc);
             RTT_LOG_INFO("End of error report");
            break;
        }
        default:
            RTT_LOG_INFO("UNKNOWN FAULT at 0x%08X", pc);
            break;
    }
#endif

    NRF_BREAKPOINT_COND;
    // On assert, the system can only recover with a reset.

#ifndef DEBUG
    RTT_LOG_INFO("System reset");
    nrf_delay_ms(1);
    NVIC_SystemReset();
#else
  app_error_save_and_stop(id, pc, info);
#endif // DEBUG
}

/**@brief Function for handling asserts in the SoftDevice.
 *
 * @details This function is called in case of an assert in the SoftDevice.
 *
 * @warning This handler is only an example and is not meant for the final product. You need to analyze
 *          how your product is supposed to react in case of assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num     Line number of the failing assert call.
 * @param[in] p_file_name  File name of the failing assert call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{ // Function for handling asserts in the SoftDevice
  RTT_LOG_INFO("ERR: assert_nrf_callback(), line num %d, file name %s", line_num, nrf_log_push((char *)p_file_name));
  NRF_BREAKPOINT_COND;
//    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

#if(WDT_EN)
void wdt_timer_timeout(void *args)
{ // сброс WDT-таймера
  WDT_Reset();
}
#endif // WDT_EN

// ############################################  MAIN  ################################################
int main(void)
{
    ret_code_t ret;
  
    // Initialize.
#if(RTTLOG_EN)
    log_init();
#endif // RTTLOG_EN
   
    ret = nrf_drv_power_init(NULL);
    APP_ERROR_CHECK(ret);
    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);
  
    // запуск суперзадачи
    if (pdPASS != xTaskCreate(super_task_thread, "SUPERTASK", SUPERTASK_STACK_SIZE, NULL, SUPERTASK_PRIORITY, &m_superTask))
    {
      RTT_LOG_INFO("MAIN: Can't create super_task_thread");
      APP_ERROR_CHECK(NRF_ERROR_NO_MEM); // генерю ошибку
    }
    
#if(BLE_EN)
    ret = blePMInit();
    if(ret != NRF_SUCCESS) RTT_LOG_INFO("MAIN: Can't init PM");
#endif // BLE_EN
    
    // Activate deep sleep mode.
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    // Start FreeRTOS scheduler.
    vTaskStartScheduler();

    for (;;)
    {
       APP_ERROR_HANDLER(NRF_ERROR_FORBIDDEN);
    }
}

