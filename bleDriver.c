/*
Драйвер для работы с BLE
Поддерживает одно входящее соединение
Поддерживаются сервисы NUS, BAS и DIS
Управление соединениями происходит через conn_handle

17.07.2023 - Начало работы


СДЕЛАТЬ


ОГРАНИЧЕНИЯ
- пока поддерживается только одно соединение (NRF_SDH_BLE_TOTAL_LINK_COUNT должно быть равно 1)


ВОЗМОЖНОСТИ
- для всех соединений поддерживается NUS
- для каждого установленного соединения верхний уровень может задать приемный и/или передающий буфер для обмена через NUS

ОСОБЕННОСТИ
- в целях безопасности, ребондинг запрещен (повторный бондинг ранее забинденных устройств, если они потеряли информацию о бондинге)
- blePMInit() можно выполнить до инициализации драйвера bleInit() При инициализации выделяется окло 6к памяти, при вызове из задачи будет ошибка инициализации (непонятно почему)
- событие BLE_EVENT_TYPE_CONNECT возникает ТОЛЬКО ПОСЛЕ установления защищенного подключения (после успешной авторизации)
- ВНИМАНИЕ! при попытке нового подключения (на телефоне: ожидания ответа на вопрос на установку соединения) возможно использование всех сервисов! Если это нужно для целей безопасности,
защита строится на более высоком уровне, а не в этом драйвере
- на верхнем уровне, в обработчике события BLE_EVENT_TYPE_CONNECT необходимо задать приемный и передающий буферы сервиса NUS (с помощью bleSetNusBuffers), иначе даже событий не будет
- при возникновении события PM_EVT_STORAGE_FULL, на уровне SDK происходит запуск функции очистки флеша от мусора и удаление самого редко используемого бондинга (если включено ранжирование
подключающихся устройств)
- количество передающих буферов определяется константой NRF_SDH_BLE_GAP_EVENT_LENGTH (задается в sdk_config.h), больше 40 ставить не имеет смысла на битовой скорости 1М (при таких
настройках скорость передачи получилась около 670 кБит/сек)

*/

#include "bleDriver.h"

#include "settings.h"
#include "errors.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "app_timer.h"
#include "ble.h"
#include "app_util.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_db_discovery.h"
#include "ble_nus.h"
#include "ble_nus_c.h"
#include "ble_dis.h"
#include "ble_bas.h"
#include "ble_conn_state.h"
#include "fds.h"
#include "nrf_crypto.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_lesc.h"
#include "nrf_ble_qwr.h"
#include "ble_lbs.h"
#include "ble_lbs_c.h"


// FreeRTOS
//#include "FreeRTOS.h"
#include "task.h"
#include "nrf_sdh_freertos.h"

// НАСТРОЙКИ МОДУЛЯ ************************************
#define BLE_TX_ERROR_MAX          10    // максимальное количество ошибок в процессе передачи
#define BLE_TX_ERROR_TIMEOUT_MS   100   // таймаут следующей попытки передачи при возникновении ошибки (скорость передачи по другим открытым соединениям также замедлится)
#define PASS_KEY_DEF              "123456" // дефолтный ключ для аутентификации при сопряжении (если выбран этот режим)

#if(RTTLOG_EN)
#include "logger_freertos.h"
#define RTT_LOG_EN                0     // включить лог через RTT
#define RTT_DEBUG_EN              0     // дебажные сообщения
#else
#define RTT_LOG_EN 0
#endif // RTTLOG_EN

#if(RTT_LOG_EN)
#include "logger_freertos.h"
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
// *****************************************************

    
#ifndef NUSTX_PRIORITY
#define NUSTX_PRIORITY                  2                                               // приоритет потока передача данных по каналу BLE
#endif

#ifndef NUSTX_STACK_SIZE
#define NUSTX_STACK_SIZE                256                                             // стек для процесса передачи данных по BLE
#endif


// Информация об устройстве находится в settings.h


#define APP_COMPANY_IDENTIFIER          0xFFFF                                          // код производителя для BLE пакетов (0xFFFF используется, если кода еще нет)   

//НЕ ПОДДЕРЖИВАЕТСЯ, НАЧИНАЯ С SDK 15 #define LESC_DEBUG_MODE 0 /**< Set to 1 to use the LESC debug keys. The debug mode allows you to use a sniffer to inspect traffic. */
// можно выйти из положения путем задания статического ключа в функции nrf_crypto_rng_vector_generate_no_mutex модуля nrf_crypto_rng.c

#define LESC_MITM_NC                    1                                               /**< Use MITM (Numeric Comparison). */
#define APP_BLE_CONN_CFG_TAG            1                                               /**< Tag that identifies the SoftDevice BLE configuration. */
//#define NRF_BLE_GQ_QUEUE_SIZE           8                                               /* Queue size for BLE GATT Queue module */

#if LESC_MITM_NC
  #define SEC_PARAMS_MITM               1                                               /**< Man In The Middle protection required. */
  #define SEC_PARAMS_IO_CAPABILITIES    BLE_GAP_IO_CAPS_DISPLAY_ONLY                    // при первом подключении к устройству будет необходимо ввести пин-код
#else
  #define SEC_PARAMS_MITM               0                                               /**< Man In The Middle protection required. */
  #define SEC_PARAMS_IO_CAPABILITIES    BLE_GAP_IO_CAPS_NONE                            /**< No I/O caps. */
#endif // LESC_MITM_NC

#define SEC_PARAMS_LESC                 1                                               /**< LE Secure Connections pairing required. */
#define SEC_PARAMS_KEYPRESS             0                                               /**< Keypress notifications not required. */
#define SEC_PARAMS_OOB                  0                                               /**< Out Of Band data not available. */
#define SEC_PARAMS_MIN_KEY_SIZE         7                                               /**< Minimum encryption key size in octets. */
#define SEC_PARAMS_MAX_KEY_SIZE         16                                              /**< Maximum encryption key size in octets. */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                           /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(10000)                          /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    5                                               /**< Number of attempts before giving up the connection parameter negotiation. */

#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(500, UNIT_10_MS)                 /**< Connection supervisory timeout (500 ms). Supervision Timeout uses 10 ms units. */

#define SLAVE_LATENCY                   0                                               /**< Slave latency. */
/**@brief   Priority of the application BLE event handler.
 * @note    There is no need to modify this value.
 */
#define APP_BLE_OBSERVER_PRIO           3

#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                      /**< UUID type for the Nordic UART Service (vendor specific). */



typedef struct
{ // информация о подключении
  bool                    is_connected;                   // флаг текущего подключения
  ble_gap_addr_t          address;                        // МАК адрес подключенного устройства (только для исходящих)
  uint16_t                nus_max_data_len;               /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
  uint8_t                 tx_data[BLE_NUS_MAX_DATA_LEN];  // буфер для временного хранения данных во время передачи
  uint16_t                tx_data_len;                    // длина данных во временном буфере
  bool                    is_bonded;                      // флаг предварительно забинденного устройства
  StreamBufferHandle_t    tx_stream_buff_handle;          // хендл буфера для передачи
  StreamBufferHandle_t    rx_stream_buff_handle;          // хендл буфера для приема
  SemaphoreHandle_t       tx_done_sema;                   // семафор окончания передачи
  uint32_t                tx_error_cnt;                   // счетчик ошибок передачи
} ble_conn_t;

typedef struct // НАСТРАИВАЕМЫЕ ПАРАМЕТРЫ ЭДВЕРТАЙЗИНГА
{
  char          device_name[15];  // имя при эдвертайзинге (размер с потолка: чем меньше, тем больше войдет другой инфы)
  uint32_t      interval_ms;      // периодичность пакетов в мс
  uint32_t      duration_ms;      // длительность эдвертайзинга в мс (если =0, значит бессрочно)
  blePwr_t      pwrAdv;           // мощность в процессе эдвертайзинга
  uint32_t      min_conn_interval_ms; // минимальный интервал после подключения
  uint32_t      max_conn_interval_ms; // максимальный интервал после подключения
  blePwr_t      pwrConn;          // мощность после установления соединения
  bool          conn_en;          // флаг разрешения подключения
} adv_params_t;



// NRF_BLE_GQ_DEF(m_ble_gatt_queue,                          /**< BLE GATT Queue instance. */
//                NRF_SDH_BLE_CENTRAL_LINK_COUNT,
//                NRF_BLE_GQ_QUEUE_SIZE);
NRF_BLE_GATT_DEF(m_gatt);                                 /**< GATT module instance. */
NRF_BLE_QWRS_DEF(m_qwr, NRF_SDH_BLE_TOTAL_LINK_COUNT);    /**< Context for the Queued Write module. По сути, это массив, количество элементов соответствует NRF_BLE_LINK_COUNT */
// ***** ADVERTISING *****
static ble_gap_adv_params_t m_gap_adv_params;               /**< Parameters to be passed to the stack when starting advertising. */
static uint8_t              m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET; /**< Advertising handle used to identify an advertising set. */
static uint8_t              m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];  /**< Buffer for storing an encoded advertising set. */
static uint8_t              m_enc_srdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];  /**< Buffer for storing an encoded scan set. */ 
// ***********************
// BLE_DB_DISCOVERY_DEF(m_db_disc);                          /**< Database discovery module instance. */
BLE_BAS_DEF(m_bas);                                       /**< Battery service instance. */
BLE_NUS_DEF(m_nus, NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);    // NUS Perephiral


static ble_conn_t     m_connected_peers[NRF_BLE_LINK_COUNT];    /**< Array of connected peers. */
 // используется при сканировании для получения дополнительной информации
static TaskHandle_t   m_nus_tx_thread = NULL;                   // хендлер задачи передачи данных по каналам NUS
static bool           m_paring_en = false;                      // флаг разрешения спаривания с новыми устройствами
static adv_params_t   m_adv_params;                             // параметры эдвертайзинга


/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = m_enc_srdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX

    }
};

#if RTT_LOG_EN || RTT_DEBUG_EN
static char * roles_str[] =
{
    "INVALID_ROLE",
    "PERIPHERAL",
    "CENTRAL",
};
#endif

// УНИВЕРСАЛЬНЫЙ ДОПОЛНИТЕЛЬНЫЙ ОБРАБОТЧИКИК ВЕРХНЕГО УРОВНЯ (передаются различные события для дополнительной обработки на верхнем уровне)
static bleDriverCallback_t m_callback = NULL;                   // дополнительный обработчик для различных эвентов (ble_perepheral_evt_handler и др.)



// СЕРВИСЫ
static ble_uuid_t m_adv_uuids[] =                               /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE},              // Nordic Uart Service
    {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE},              // информация о батарее питания
    {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},   // информация об устройстве
};


// ОПРЕДЕЛЕНИЯ ВНУТРЕННИХ ФУНКЦИЙ
// обработчики ошибок
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name); // Function for handling asserts in the SoftDevice
static void nrf_qwr_error_handler(uint32_t nrf_error);        // Function for handling Queued Write Module errors
static void conn_params_error_handler(uint32_t nrf_error);    // Function for handling errors from the Connection Parameters module
static ret_code_t peer_manager_init_peripheral(void);         // инициализация менеджера пиров для роли peripheral
static void pm_evt_handler(pm_evt_t const * p_evt);           // Function for handling Peer Manager events
// инициализация сервисев
static ret_code_t battery_service_init(void);                 // инициализация сервиса передачи информации о батареи
// объекты NUS
static ret_code_t nus_init(void);                             // инициализация сервиса NUS perephiral
static void nus_data_handler(ble_nus_evt_t * p_evt);          // Function for handling the data from the Nordic UART Service
static ret_code_t services_init(void);                        // инициализация ВСЕХ сервисов
// вспомогательные функции
static bool is_already_connected(ble_gap_addr_t const * p_connected_adr); // возвращает TRUE, если устройство с таким адресом уже подключено
static void multi_qwr_conn_handle_assign(uint16_t conn_handle); // Function for assigning new connection handle to the available instance of QWR module
static ret_code_t qwr_init(void); // Function for initializing the Queued Write instances
static uint16_t setDefPassKey(const char *passKey); // установка дефолтного пароля для сопряжения
// обработчики событий от BLE
static void on_ble_evt(uint16_t conn_handle, ble_evt_t const * p_ble_evt); // обработчик BLE Stack эвентов
static void on_ble_peripheral_evt(ble_evt_t const * p_ble_evt); // обработчик BLE Stack эвентов относящихся к перефирийному устройству
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context); // Function for handling BLE events
// стек BLE, GAP, GATT
static ret_code_t ble_stack_init(void); // инициализация BLE стека
static ret_code_t gap_params_init(const char *deviceName, uint32_t min_conn_interval, uint32_t max_conn_interval); // Function for initializing the GAP
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt); // Function for handling events from the GATT library
static ret_code_t gatt_init(void); // Function for initializing the GATT module
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt); // Function for handling the Connection Parameters Module
static ret_code_t conn_params_init(void); // Function for initializing the Connection Parameters module



// >>>>>>>>>>>>>>> ОБРАБОТЧИКИ ОШИБОК >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

/**
 * Function for handling Queued Write Module errors.
 *
 * A pointer to this function is passed to each service that may need to inform the
 * application about an error.
 *
 * nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{ // Function for handling Queued Write Module errors
  RTT_LOG_INFO("bleDriver, rf_qwr_error_handler(), err 0x%04X", nrf_error);
}

/**
 * Function for handling errors from the Connection Parameters module.
 *
 * nrf_error  Error code that contains information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{ // Function for handling errors from the Connection Parameters module
  RTT_LOG_INFO("bleDriver, conn_params_error_handler(), err 0x%04X", nrf_error);
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


// >>>>>>>>>>>>>>> PEER MANAGER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

static ret_code_t peer_manager_init_peripheral(void)
{ // инициализация менеджера пиров для роли peripheral (входящие соединения)
    ble_gap_sec_params_t sec_params;

    memset(&sec_params, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    // разрешаю бондинг, когда подключаются ко мне
    sec_params.bond           = 1;
    sec_params.mitm           = SEC_PARAMS_MITM;
    sec_params.lesc           = SEC_PARAMS_LESC;
    sec_params.keypress       = SEC_PARAMS_KEYPRESS;
    sec_params.io_caps        = SEC_PARAMS_IO_CAPABILITIES;
    sec_params.oob            = SEC_PARAMS_OOB;
    sec_params.min_key_size   = SEC_PARAMS_MIN_KEY_SIZE;
    sec_params.max_key_size   = SEC_PARAMS_MAX_KEY_SIZE;
    sec_params.kdist_own.enc  = 1;
    sec_params.kdist_own.id   = 1;
    sec_params.kdist_peer.enc = 1;
    sec_params.kdist_peer.id  = 1;

    // устанавливаю заданные выше параметры
    return pm_sec_params_set(&sec_params);
}

/**
 * Function for handling Peer Manager events.
 *
 * [in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{ // Function for handling Peer Manager events
    pm_handler_on_pm_evt(p_evt); // обработка событий внутри peer_manager_handler.c
    pm_handler_disconnect_on_sec_failure(p_evt); // разрыв соединения, если прилетит событие PM_EVT_CONN_SEC_FAILED
    pm_handler_flash_clean(p_evt); // управляет размещением записей на флешке
  
    uint8_t role = ble_conn_state_role(p_evt->conn_handle); // получаю РОЛЬ подключенного устройства по хендлу

    switch (p_evt->evt_id)
    {
        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            
            break;
        
        case PM_EVT_CONN_SEC_FAILED: // ошибка при устновлении защищенного содинения
          RTT_LOG_DEBUG("PM_EVT: PM_EVT_CONN_SEC_FAILED");
        
          // удаляю информацию о сопряжении, если мы в режиме паринга
          if(m_paring_en)
            pm_peer_delete(p_evt->peer_id);
          
          // отправляю уведомление об ОШИБКИ установлении защищенного соединения
        break;
        
        case PM_EVT_CONN_SEC_SUCCEEDED:
          // установлено защищенное соединение с ранее забинденным устройством
          RTT_LOG_DEBUG("PM_EVT: PM_EVT_CONN_SEC_SUCCEEDED");
          // успешное подключение в качестве перефирийного
          // вызываю дополнительный обработчик
          if(m_callback)
          {
            bleCallback_t args;
            memset(&args, 0, sizeof(args));
            args.conn_handle = (int8_t)p_evt->conn_handle;
            args.evtType = BLE_EVENT_TYPE_CONNECT;
            args.evtSrc = BLE_EVENT_SRC_PERIPHERAL;
            m_callback(&args);
          }
        break;

        case PM_EVT_BONDED_PEER_CONNECTED:
          RTT_LOG_DEBUG("PM_EVT: PM_EVT_BONDED_PEER_CONNECTED");
          m_connected_peers[p_evt->conn_handle].is_bonded = true; // флаг сбрасывается при отключении устройства (вся структура заполняется нулями)
        break;
        
        case PM_EVT_STORAGE_FULL: // переполнение хранилища информации о бондинге
          RTT_LOG_INFO("PM_EVT: PM_EVT_STORAGE_FULL");
        break;
        
        case PM_EVT_FLASH_GARBAGE_COLLECTED: // очистка свободного места успешно завершена
          RTT_LOG_INFO("PM_EVT: PM_EVT_FLASH_GARBAGE_COLLECTED");
        break;
        
        case PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED: // произошла ошибка при очистке свободного места
        {
          RTT_LOG_INFO("PM_EVT: PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED");
          // удаляю запись, которая реже всего использовалась
          pm_peer_id_t id;
          // если произошла ошибка при определении такой записи, то выбираю самую первую
          if(NRF_SUCCESS != pm_peer_ranks_get(NULL, NULL, &id, NULL)) id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
          pm_peer_delete(id);
          // запускаю сборщик мусора
          fds_gc();
        }
        break;
        
        case PM_EVT_CONN_SEC_START:
          RTT_LOG_DEBUG("PM_EVT: PM_EVT_CONN_SEC_START");
        break;
        
        case PM_EVT_ERROR_UNEXPECTED:
          RTT_LOG_INFO("PM_EVT: PM_EVT_ERROR_UNEXPECTED");
        break;
        
        case PM_EVT_CONN_SEC_CONFIG_REQ:
          RTT_LOG_DEBUG("PM_EVT: PM_EVT_CONN_SEC_CONFIG_REQ");
        break;
        
        case PM_EVT_CONN_SEC_PARAMS_REQ:
          RTT_LOG_DEBUG("PM_EVT: PM_EVT_CONN_SEC_PARAMS_REQ");
          /** Security parameters (@ref ble_gap_sec_params_t) are needed for an ongoing security procedure. 
          * Reply with @ref pm_conn_sec_params_reply before the event handler returns. 
          * If no reply is sent, the parameters given in @ref pm_sec_params_set are used. 
          * If a peripheral connection, the central's sec_params will be available in the event. 
          */
          if((role == BLE_GAP_ROLE_PERIPH) && (!m_paring_en)) 
          { // отправляю ответ, что паринг не поддерживается, если паринг запрещен и роль перефирийного устройства
            // этот ответ не влияет на работу ранее забинденных устройств
            RTT_LOG_INFO("pm_evt_handler: BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPPORTED");
            // выполнение этого кода приведет к отказу в паринге (бондинге) и разрыву соединения со следующим уведомлением:
            // Paring Not Supported
            sd_ble_gap_sec_params_reply(p_evt->conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, p_evt->params.conn_sec_params_req.p_context);
          }
        break;

        default:
            break;
    }
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>> СЕРВИСЫ >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

static ret_code_t battery_service_init(void)
{ // инициализация сервиса передачи информации о батареи
  ble_bas_init_t     bas_init;
  
  // Initialize Battery Service.
  memset(&bas_init, 0, sizeof(bas_init));

  // Here the sec level for the Battery Service can be changed/increased.
  bas_init.bl_rd_sec        = SEC_OPEN;
  bas_init.bl_cccd_wr_sec   = SEC_OPEN;
  bas_init.bl_report_rd_sec = SEC_OPEN;

  bas_init.evt_handler          = NULL;
  bas_init.support_notification = true;
  bas_init.p_report_ref         = NULL;
  bas_init.initial_batt_level   = 100; // значение обновляется через внешнюю функцию

  return ble_bas_init(&m_bas, &bas_init);
}

static ret_code_t nus_init(void) 
{ // инициализация сервиса NUS perephiral
  ble_nus_init_t     nus_init;
  
  memset(&nus_init, 0, sizeof(nus_init));

  nus_init.data_handler = nus_data_handler;

  return(ble_nus_init(&m_nus, &nus_init));
}

/**
 * Function for handling the data from the Nordic UART Service.
 *
 * This function processes the data received from the Nordic UART BLE Service and sends
 * it to the USBD CDC ACM module.
 *
 * [in] p_evt Nordic UART Service event.
 */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    uint16_t conn_handle = p_evt->conn_handle;
    switch(p_evt->type)
    {
      case BLE_NUS_EVT_RX_DATA: // приняты какие-то данные
      {
        RTT_LOG_DEBUG("BLE_NUS_EVT_RX_DATA, conn_handle = %d", conn_handle);
        if(conn_handle >= NRF_BLE_LINK_COUNT) break;
        if(m_connected_peers[conn_handle].rx_stream_buff_handle != NULL)
        { // приемный буфер был задан
         // отправляю полученные данные в очередь
          if(xStreamBufferSpacesAvailable(m_connected_peers[conn_handle].rx_stream_buff_handle) >= p_evt->params.rx_data.length)
          {
            xStreamBufferSendFromISR(m_connected_peers[conn_handle].rx_stream_buff_handle, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length, NULL);
          }else{
            RTT_LOG_INFO("BLE: rx data stream ofv");
          }

          //RTT_LOG_DEBUG("BLE: RX data %d bytes", p_evt->params.rx_data.length);
          // вызываю callback
          if(m_callback)
          {
            bleCallback_t args;
            memset(&args, 0, sizeof(args));
            args.conn_handle = conn_handle;
            args.evtType = BLE_EVENT_TYPE_NUS_RX;
            args.evtSrc = BLE_EVENT_SRC_PERIPHERAL;
            m_callback(&args);
          }
        } // if
      }
      break;
      
      case BLE_NUS_EVT_TX_RDY: // данные переданы
      {
        // отправляю эвент окончания передачи
        RTT_LOG_DEBUG("BLE_NUS_EVT_TX_RDY, conn_handle = %d", conn_handle);
        vTaskNotifyGiveFromISR(m_nus_tx_thread, NULL); // данные могут еще быть, отправляю нотификатор
      }       
      break;
      
      
      default:
        break;
    }
}

static ret_code_t services_init(void)
{ // инициализация сервисов
  ERROR_CHECK(battery_service_init());

  return nus_init();
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>> ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

/**
 * Function for checking whether a link already exists with a newly connected peer.
 *
 * This function checks whether the newly connected device is already connected.
 *
 * [in]   p_connected_evt Bluetooth connected event.
 * return  
 *  True if the peer's address is found in the list of connected peers, false otherwise.
 */
static bool is_already_connected(ble_gap_addr_t const * p_connected_adr)
{ // возвращает TRUE, если устройство с таким адресом уже подключено
    for (uint32_t i = 0; i < NRF_BLE_LINK_COUNT; i++)
    {
        if (m_connected_peers[i].is_connected)
        {
            if (m_connected_peers[i].address.addr_type == p_connected_adr->addr_type)
            {
                if (memcmp(m_connected_peers[i].address.addr,
                           p_connected_adr->addr,
                           sizeof(m_connected_peers[i].address.addr)) == 0)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * Function for assigning new connection handle to the available instance of QWR module.
 *
 * [in] conn_handle New connection handle.
 */
static void multi_qwr_conn_handle_assign(uint16_t conn_handle)
{ // Function for assigning new connection handle to the available instance of QWR module
    for (uint32_t i = 0; i < NRF_BLE_LINK_COUNT; i++)
    { // перебираю все слоты в поиске "пустого"
        if (m_qwr[i].conn_handle == BLE_CONN_HANDLE_INVALID)
        { // свободный слот найден, назначаю ему хендл подключенного устройства
            ret_code_t err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr[i], conn_handle);
            if(err_code != NRF_SUCCESS) RTT_LOG_INFO("ERR: multi_qwr_conn_handle_assign err = %d", err_code);
            break;
        }
    }
}

/**
 * Function for initializing the Queued Write instances.
 */
static ret_code_t qwr_init(void)
{ // Function for initializing the Queued Write instances
    nrf_ble_qwr_init_t qwr_init_obj = {0};

    qwr_init_obj.error_handler = nrf_qwr_error_handler;

    for (uint32_t i = 0; i < NRF_BLE_LINK_COUNT; i++)
    { // начальная инициализация массива подключенных устройств
        ERROR_CHECK(nrf_ble_qwr_init(&m_qwr[i], &qwr_init_obj));
    }
    return NRF_SUCCESS;
}

//static uint8_t *ble_advdata_manuf_data_find(const ble_data_t *p_adv_data, uint8_t *manuf_data_length)
//{ // поиск manufacturer specific data в эдвертайзинг пакете
//    uint16_t data_offset = 0;
//    *manuf_data_length = ble_advdata_search(p_adv_data->p_data,
//                                         p_adv_data->len,
//                                         &data_offset,
//                                         BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA);

//    return &p_adv_data->p_data[data_offset];
//}

static uint16_t setDefPassKey(const char *passKey)
{ // установка дефолтного пароля для сопряжения
  ble_opt_t ble_opt;
  ble_opt.gap_opt.passkey.p_passkey = (const uint8_t *)passKey;
  return(sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &ble_opt));
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>> ОБРАБОЧИКИ СОБЫТИЙ ОТ BLE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

/**
 * Function for handling BLE Stack events that are common to both the central and peripheral roles.
 * 
 * [in] conn_handle Connection Handle.
 * [in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(uint16_t conn_handle, ble_evt_t const * p_ble_evt)
{ // обработчик BLE Stack эвентов
    uint16_t    role = ble_conn_state_role(conn_handle); // получаю РОЛЬ подключенного устройства по хендлу

    pm_handler_secure_on_connection(p_ble_evt); // обработчик событий менеджера пиров 

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED: // подключено новое устройство
            RTT_LOG_INFO("BLE: %s: on_ble_evt: BLE_GAP_EVT_CONNECTED, conn_handle = %d", nrf_log_push(roles_str[role]), conn_handle);
            m_connected_peers[conn_handle].is_connected = true; // устанавливаю флаг подключения в массиве подключенных устройств
            m_connected_peers[conn_handle].address = p_ble_evt->evt.gap_evt.params.connected.peer_addr; // копирую адрес подключенного устройства
            multi_qwr_conn_handle_assign(conn_handle); 
            // вызываю дополнительный обработчик
            if(m_callback)
            {
              bleCallback_t args;
              memset(&args, 0, sizeof(args));
              args.conn_handle = (int8_t)conn_handle;
              args.evtType = BLE_EVENT_TYPE_GAP;
              args.evtSrc = BLE_EVENT_SRC_PERIPHERAL;
              args.evt.gapEvt = BLE_GAP_EVT_CONNECTED;
              m_callback(&args);
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED: // отключение устройства
            RTT_LOG_INFO("BLE: %s: on_ble_evt: BLE_GAP_EVT_DISCONNECTED, conn_handle = %d", nrf_log_push(roles_str[role]), conn_handle);
            // "удаляю" устройство из массива подключенных вместе со всеми настройками буферов
            memset(&m_connected_peers[conn_handle], 0x00, sizeof(m_connected_peers[0]));
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST: // запрос секьюрных параметров
            RTT_LOG_DEBUG("BLE: %s: on_ble_evt: BLE_GAP_EVT_SEC_PARAMS_REQUEST, conn_handle = %d", nrf_log_push(roles_str[role]), conn_handle);
            break;

        case BLE_GAP_EVT_PASSKEY_DISPLAY: // требуется подтверждение сопряжения
        {
            char passkey[BLE_GAP_PASSKEY_LEN + 1]; // буфер для хранения пароля
            // копирую пароль, который требуется подтвердить
            memcpy(passkey, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, BLE_GAP_PASSKEY_LEN);
            passkey[BLE_GAP_PASSKEY_LEN] = 0x00;
            RTT_LOG_INFO("BLE: %s: BLE_GAP_EVT_PASSKEY_DISPLAY: passkey=%s match_req=%d",
                         nrf_log_push(roles_str[role]),
                         nrf_log_push(passkey),
                         p_ble_evt->evt.gap_evt.params.passkey_display.match_request);
        }
        break;

        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
            RTT_LOG_INFO("BLE: %s: BLE_GAP_EVT_AUTH_KEY_REQUEST", nrf_log_push(roles_str[role]));
            break;

        case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
            RTT_LOG_INFO("BLE: %s: BLE_GAP_EVT_LESC_DHKEY_REQUEST", nrf_log_push(roles_str[role]));
            break;

         case BLE_GAP_EVT_AUTH_STATUS: // результат авторизации
             RTT_LOG_INFO("BLE: %s: BLE_GAP_EVT_AUTH_STATUS: status=0x%x bond=0x%x lv4: %d kdist_own:0x%x kdist_peer:0x%x",
                          nrf_log_push(roles_str[role]),
                          p_ble_evt->evt.gap_evt.params.auth_status.auth_status,
                          p_ble_evt->evt.gap_evt.params.auth_status.bonded,
                          p_ble_evt->evt.gap_evt.params.auth_status.sm1_levels.lv4,
                          *((uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_own),
                          *((uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_peer));
         
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            RTT_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            ret_code_t err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            if(err_code != NRF_SUCCESS) RTT_LOG_INFO("ERR: BLE_GAP_EVT_PHY_UPDATE_REQUEST err = %d", err_code);
        } break;

        default:
            // No implementation needed.
            break;
    }
}


/**
 * Function for handling BLE Stack events that involves peripheral applications. 
 *
 * [in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_peripheral_evt(ble_evt_t const * p_ble_evt)
{ // обработчик BLE Stack эвентов относящихся к перефирийному устройству
  // процесс внешнего подключения приведет к выходу из эдвертайзинга
  // так же внешний процесс подключения может закончиться ошибкой, что приведет к событию BLE_GAP_EVT_DISCONNECTED, в этом случае эдвертайзинга тоже не будет

    ret_code_t err_code;
    uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
  
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
        {
          RTT_LOG_INFO("PERIPHERAL: BLE_GAP_EVT_CONNECTED, handle %d.", conn_handle);
          // меняю максимальную мощность BLE (по дефолту стоит 0 дБм)
          sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, conn_handle, m_adv_params.pwrConn);
        }
        break;

        case BLE_GAP_EVT_DISCONNECTED:
        {
            RTT_LOG_INFO("PERIPHERAL: BLE_GAP_EVT_DISCONNECTED, handle %d, reason 0x%x.",
                         conn_handle,
                         p_ble_evt->evt.gap_evt.params.disconnected.reason);
        
            // вызываю дополнительный обработчик
            if(m_callback)
            {
              bleCallback_t args;
              memset(&args, 0, sizeof(args));
              args.conn_handle = (int8_t)conn_handle;
              args.evtType = BLE_EVENT_TYPE_DISCONNECT;
              args.evtSrc = BLE_EVENT_SRC_PERIPHERAL;
              m_callback(&args);
            }
        }
        break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            RTT_LOG_DEBUG("PERIPHERAL: BLE_GATTC_EVT_TIMEOUT");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if(err_code != NRF_SUCCESS) RTT_LOG_INFO("ERR: BLE_GATTC_EVT_TIMEOUT-1 err = %d", err_code);
        break;

        case BLE_GAP_EVT_ADV_SET_TERMINATED: // таймаут эдвертайзинга
        {
          RTT_LOG_DEBUG("PERIPHERAL: BLE_GAP_EVT_ADV_SET_TERMINATED");
        
          // вызываю дополнительный обработчик
          if(m_callback)
          {
            bleCallback_t args;
            memset(&args, 0, sizeof(args));
            args.conn_handle = (int8_t)conn_handle;
            args.evtType = BLE_EVENT_TYPE_GAP;
            args.evtSrc = BLE_EVENT_SRC_PERIPHERAL;
            args.evt.gapEvt = BLE_GAP_EVT_ADV_SET_TERMINATED;
            m_callback(&args);
          }
        }
        break;
        
        case BLE_GAP_EVT_PASSKEY_DISPLAY:
        {
          char passkey[BLE_GAP_PASSKEY_LEN + 1]; // буфер для хранения пароля
          // копирую пароль, который требуется подтвердить
          memcpy(passkey, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, BLE_GAP_PASSKEY_LEN);
          passkey[BLE_GAP_PASSKEY_LEN] = 0x00;
          
          // отправляю сообщение на верхний уровень
          if(m_callback && m_paring_en)
          {
            bleCallback_t args;
            memset(&args, 0, sizeof(args));
            args.conn_handle = (int8_t)conn_handle;
            args.evtType = BLE_EVENT_TYPE_PASSWORD;
            args.evtSrc = BLE_EVENT_SRC_PERIPHERAL;
            args.evt.pswReq.spasskey = (char *)passkey;
            args.evt.pswReq.match_request = p_ble_evt->evt.gap_evt.params.passkey_display.match_request;
            m_callback(&args);
          }
        }
        break;
        
        case BLE_GAP_EVT_AUTH_STATUS: // результат авторизации
          RTT_LOG_DEBUG("PERIPHERAL: BLE_GAP_EVT_AUTH_STATUS");
         // if(p_ble_evt->evt.gap_evt.params.auth_status.auth_status == NRF_SUCCESS)
         // {  
         // }
        break;

        default:
            // No implementation needed.
            break;
    }
}


/**
 * Function for handling BLE events.
 *
 * [in]   p_ble_evt   Bluetooth stack event.
 * [in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{ // Function for handling BLE events
    uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    uint16_t role        = ble_conn_state_role(conn_handle);

    // проверка повторных подключений
    if (    (p_ble_evt->header.evt_id == BLE_GAP_EVT_CONNECTED)
        &&  (is_already_connected(&p_ble_evt->evt.gap_evt.params.connected.peer_addr)))
    { // пришло событие о новом подключении, а устройство с таким адресом уже подключено
        RTT_LOG_INFO("BLE: %s: Already connected to this device as %s (handle: %d), disconnecting.",
                     (role == BLE_GAP_ROLE_PERIPH) ? "PERIPHERAL" : "CENTRAL",
                     (role == BLE_GAP_ROLE_PERIPH) ? "CENTRAL"    : "PERIPHERAL",
                     conn_handle);
        // разрываю повторное подключение
        (void)sd_ble_gap_disconnect(conn_handle, BLE_HCI_CONN_FAILED_TO_BE_ESTABLISHED);

        // Do not process the event further.
        return;
    }
    
    if(role == BLE_GAP_ROLE_PERIPH)
    { // в зависимости от роли, делаю настройку пир-менеджера
      peer_manager_init_peripheral();
    }else if(role == BLE_GAP_ROLE_CENTRAL) {
      RTT_LOG_INFO("BLE: Central role is not supported");
      (void)sd_ble_gap_disconnect(conn_handle, BLE_HCI_CONN_FAILED_TO_BE_ESTABLISHED);
      return;
    }

    // вызываю обработчик эвентов BLE стека
    on_ble_evt(conn_handle, p_ble_evt);
    
    // исключения для BLE_GAP_EVT_ADV_SET_TERMINATED и BLE_GAP_EVT_ADV_REPORT сделаны потому, что они идут вместе с role = BLE_GAP_ROLE_INVALID, так как соединение еще не установлено
    // TODO перенести требуемые события, для которых role = BLE_GAP_ROLE_INVALID, в отдельный обработчик
    // в зависимости от роли, вызываю обработчики в зависимости от роли подключения
    if ((role == BLE_GAP_ROLE_PERIPH) || (p_ble_evt->header.evt_id == BLE_GAP_EVT_ADV_SET_TERMINATED))
    {
        // Manages peripheral LEDs.
        on_ble_peripheral_evt(p_ble_evt);
    }
}


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>> СТЕК BLE, GAP, GATT >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

/**
 * Function for initializing the BLE stack.
 *
 * Initializes the SoftDevice and the BLE event interrupts.
 */
static ret_code_t ble_stack_init(void)
{ // инициализация BLE стека
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();  // запрос на разрешение Softdevice
    ERROR_CHECK(err_code);

     // Configure the BLE stack by using the default settings.
    // Fetch the start address of the application RAM.
    // Проверка достаточности выделенной для Softdevice RAM (завист от количества подключений)
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
  
    return NRF_SUCCESS;
}

/**
 * Function for the GAP initialization.
 *
 * This function will set up all the necessary GAP (Generic Access Profile) parameters of
 * the device. It also sets the permissions and appearance.
 */
ret_code_t gap_params_init(const char *deviceName, uint32_t min_conn_interval, uint32_t max_conn_interval)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) deviceName,
                                          strlen((const char*)deviceName));
    ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = min_conn_interval;
    gap_conn_params.max_conn_interval = max_conn_interval;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    return(err_code);
}

/**
 * Function for handling events from the GATT library. 
*/
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{ // Function for handling events from the GATT library
  
    if (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)
    {
      RTT_LOG_INFO("BLE: ATT MTU exchange completed.");
      
      m_connected_peers[p_evt->conn_handle].nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
      RTT_LOG_INFO("BLE: Ble NUS max data length set to 0x%X(%d)", m_connected_peers[p_evt->conn_handle].nus_max_data_len, m_connected_peers[p_evt->conn_handle].nus_max_data_len);
    } 
}

/** Function for initializing the GATT module. */
static ret_code_t gatt_init(void)
{ // Function for initializing the GATT module

    // размер очереди hvn_tx_queue_size в m_gatt задается параметром NRF_SDH_BLE_GAP_EVENT_LENGTH в sdk_config.h

    ERROR_CHECK(nrf_ble_gatt_init(&m_gatt, gatt_evt_handler));
  
    return nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
}

/**
 * Function for handling the Connection Parameters Module.
 *
 * This function will be called for all events in the Connection Parameters Module which
 * are passed to the application.
 * All this function does is to disconnect. This could have been done by simply
 * setting the disconnect_on_fail config parameter, but instead we use the event
 * handler mechanism to demonstrate its use.
 *
 * [in]   p_evt   Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{ // Function for handling the Connection Parameters Module
    // эта функция вызывается при подключении в качестве переферийного устройства после всех служебных обменов
    RTT_LOG_INFO("BLE: on_conn_params_evt");
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
      RTT_LOG_INFO("BLE: BLE_CONN_PARAMS_EVT_FAILED");
    }
}

/** Function for initializing the Connection Parameters module. */
static ret_code_t conn_params_init(void)
{ // Function for initializing the Connection Parameters module
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL; // сюда можно добавить указатель на желаемые параметры соединения (при подключении в качестве перефирийного), иначе их будет задавать host
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID; // Start upon connection.
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    return ble_conn_params_init(&cp_init);
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>> ЗАДАЧИ >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

static void nus_tx_data_thread(void *args)
{ // поток передачи данных по каналам NUS
  /*
  ЛОГИКА РАБОТЫ:
  Этот поток обслуживает передачу данных по каналам NUS
  Процесс передачи идет по очереди до опустошения потоковых буферов; сначала данные начинают передаваться из временных буферов (на случай, если предыдущая передача завершилась ошибкой)
  За один проход основного цикла передается одна порция данных очередного пира
  Процесс стартует после приема нотификатора
  */
  
  xTaskNotifyGive(m_nus_tx_thread); // отправляю нотификатор для старта процесса
  
  for(;;)
  {
    ulTaskNotifyTake(true, portMAX_DELAY); // жду нотификатор, сброс после выхода
    
    bool all_tx_done; // флаг окончания передачи данных из всех буферов всех открытых соединений
    do{
      all_tx_done = true;
      for(uint8_t i=0; i < NRF_BLE_LINK_COUNT; i++)
      { // перебираю массив пиров
        if(m_connected_peers[i].is_connected)
        { // есть подключение
          
          while(1)
          { // цикл заполнения всех передающих буферов softdevice для увеличения скорости передачи
            // вряд ли это будет хорошо работать, если есть несколько соединений

            if(m_connected_peers[i].tx_data_len == 0)
            { // данных во временном буфере нет
              // загружаю очередную порцию данных из буфера
              m_connected_peers[i].tx_data_len = m_connected_peers[i].nus_max_data_len; // максимальная длина данных, которая может быть передана
              if(m_connected_peers[i].tx_data_len > sizeof(m_connected_peers[i].tx_data)) m_connected_peers[i].tx_data_len = sizeof(m_connected_peers[i].tx_data); // ограничение по размеру буфера
              if(m_connected_peers[i].tx_stream_buff_handle != NULL)
                m_connected_peers[i].tx_data_len = xStreamBufferReceive(m_connected_peers[i].tx_stream_buff_handle, (void *)m_connected_peers[i].tx_data, m_connected_peers[i].tx_data_len, 0);
              else m_connected_peers[i].tx_data_len = 0;
            }
            
            if(m_connected_peers[i].tx_data_len != 0)
            { // есть, что передавать
              // NRF_LOG_HEXDUMP_INFO(m_connected_peers[i].tx_data, m_connected_peers[i].tx_data_len);
              ret_code_t ret_val;
              
              // вызываю функцию передачи данных
              ret_val = ble_nus_data_send(&m_nus, m_connected_peers[i].tx_data, &m_connected_peers[i].tx_data_len, i); // передаю данные
              
              if(ret_val == NRF_SUCCESS)
              {
                m_connected_peers[i].tx_data_len = 0; // данные успешно переданы
              }else{ // в процессе передачи произошла ошибка (этот кейс нужен, чтобы работа этого потока была прервана в случае ошибок связи)
                
                if(ret_val == NRF_ERROR_RESOURCES) 
                { // процесс передачи идет, все передающие буфера заполнены полностью
                  break; // переходим к следующему устройству из списка m_connected_peers
                }

                if(m_connected_peers[i].tx_error_cnt >= BLE_TX_ERROR_MAX)
                { // превышено число допустимых ошибок, данные будут потеряны
                  m_connected_peers[i].tx_data_len = 0;
                }
                m_connected_peers[i].tx_error_cnt++;
                vTaskDelay(pdMS_TO_TICKS(BLE_TX_ERROR_TIMEOUT_MS)); // без этого таймаута для проблемных соединений, управление из этой задачи никогда не будет передано другим
              }
              
              all_tx_done = false;
            }else{
              // все данные переданы
              if(m_connected_peers[i].tx_done_sema)
                    xSemaphoreGive(m_connected_peers[i].tx_done_sema);
              break; // выход из цикла заполнения передающих буферов
            } 

          } // while

        } // if
      } // for
      taskYIELD(); // для защиты от "зависания" в этом потоке при передаче больших объемов данных или ошибках
    }while(!all_tx_done);
  }
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/*
* Начальная инициализация драйвера: инициализирует сервисы, приемный и передающий буферы
* callback - колбэк для дополнительных обработчиков различных событий
* useDefaultPass - если true, то при сопряжении будет использоваться дефолтный пассворд
* возвращает код ошибки 
* 
*/
ret_code_t bleInit(bleDriverCallback_t callback, bool useDefaultPass)
{
  // проверка ограничения: только один линк как перефирийное устройство
  ASSERT((NRF_SDH_BLE_TOTAL_LINK_COUNT != 1) || (NRF_SDH_BLE_PERIPHERAL_LINK_COUNT != 1))

  ret_code_t err_code;
  
  m_callback = callback;
  
  err_code = ble_stack_init();
  ERROR_CHECK(err_code);
  
  err_code = gatt_init();
  ERROR_CHECK(err_code);
  
  err_code = qwr_init();
  ERROR_CHECK(err_code);
  
  err_code = services_init(); 
  ERROR_CHECK(err_code);
  
  err_code = pm_register(pm_evt_handler);
  ERROR_CHECK(err_code);
  
  err_code = conn_params_init();
  ERROR_CHECK(err_code);
  
  if(useDefaultPass)
  {
    // добавляю пароль по умолчанию для аутентификации
    setDefPassKey(PASS_KEY_DEF);
  }
  
  // дефолтные параметры для эдвертайзинга
  memset(&m_adv_params, 0, sizeof(m_adv_params));
  m_adv_params.conn_en = false;
  snprintf(m_adv_params.device_name, sizeof(m_adv_params.device_name), "%s", "set_name");
  m_adv_params.duration_ms = TIME_ADV_DURATION_MS;
  m_adv_params.interval_ms = TIME_ADV_INTERVAL_MS;
  m_adv_params.max_conn_interval_ms = TIME_CONN_INTERVAL_MAX_MS;
  m_adv_params.min_conn_interval_ms = TIME_CONN_INTERVAL_MIN_MS;
  m_adv_params.pwrAdv = BLE_PWR_0;
  m_adv_params.pwrConn = BLE_PWR_0;
  
  // разрешаю использование DC-DC (должна быть сделана обвязка около микроконтроллера) для уменьшения потребления
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);

  m_paring_en = false; // запрещаю паринг новый устройств
  
  // создаю задачу по передаче данных по каналам NUS
  if(pdPASS != xTaskCreate(nus_tx_data_thread, "NUSTX", 
                            NUSTX_STACK_SIZE,
                            NULL,
                            NUSTX_PRIORITY,
                            &m_nus_tx_thread
                          )) return NRF_ERROR_NO_MEM;
  
  // Create a FreeRTOS task for the BLE stack.
  nrf_sdh_freertos_init(NULL, NULL);
  
  return NRF_SUCCESS;
}


/*
* Начальная инициализация Peer manager (требуется для резервирования памяти порядка 6к из кучи, при запуске из задачи выдает ошибку)
* Можно выполнить до начальной инициализации bleInit()
* возвращает код ошибки 
* 
*/
ret_code_t blePMInit(void)
{
  return pm_init();
}


/*
* Запуск эдвертайзинга
* device_name - имя устройства в процессе эдвертайзинга
* interval_ms - периодичность пакетов в мс
* duration_ms - длительность эдвертайзинга в мс (если =0, значит бессрочно)
* pwrAdv - мощность в процессе эдвертайзинга
* min_conn_interval_ms - минимальный интервал после подключения
* max_conn_interval_ms - максимальный интервал после подключения
* pwrConn - мощность после установления соединения
* manuf_data - указатель на данных производителя (есть ограничение по длине!)
* manuf_data_len - длина данных производителя
* conn_en - флаг разрешения подключения
* возвращает код ошибки из nrf_errors.h

Алгоритм работы:
- если имя устройства не задано (NULL), то в качестве параметров будут использованы ранее сохраненные в m_adv_params
- если эдвертайзинг уже запущен, то сначала его останавливаю, а потом запускаю с новыми параметрами
- эдвертайзинг останавливается либо по таймауту либо после подключения
*/
ret_code_t bleAdvStart(const char *device_name, uint32_t interval_ms, uint32_t duration_ms, blePwr_t pwrAdv, uint32_t min_conn_interval_ms, uint32_t max_conn_interval_ms, blePwr_t pwrConn, 
                        void *manuf_data, uint8_t manuf_data_len, bool conn_en)
{
  ble_advdata_t advdata;  // данные в эдвертайзинге
  ble_advdata_t srdata;   // данные в scan request ответе
  
  if(device_name != NULL)
  { // сохраняю новые параметры эдвертайзинга
    // преобразую имя устройства к виду DEVICE_NAME_ABCD, где ABCD - это последние 4 знака МАК-адреса
    // TEST
    //char buff[strlen(device_name) + 6]; // +5 - для добавления окончания _XXXX, где XXXX - это последние 2 байта МАК-адреса
    //uint64_t devID = (*((uint64_t*) NRF_FICR->DEVICEADDR));
    //snprintf(buff, sizeof(buff), "%s_%04X", DEVICE_NAME, (uint16_t)devID);
    //memcpy(m_adv_params.device_name, buff, sizeof(m_adv_params.device_name));
    memcpy(m_adv_params.device_name, device_name, sizeof(m_adv_params.device_name)); // TEST
    
    m_adv_params.conn_en = conn_en;
    m_adv_params.duration_ms = duration_ms;
    m_adv_params.interval_ms = interval_ms;
    m_adv_params.max_conn_interval_ms = max_conn_interval_ms;
    m_adv_params.min_conn_interval_ms = min_conn_interval_ms;
    m_adv_params.pwrAdv = pwrAdv;
    m_adv_params.pwrConn = pwrConn;
  }
  
  bleAdvStop();
  
  // преобразую входные параметры к нужным величинам
  interval_ms = MSEC_TO_UNITS(m_adv_params.interval_ms, UNIT_0_625_MS);
  duration_ms = MSEC_TO_UNITS(m_adv_params.duration_ms, UNIT_10_MS);
  min_conn_interval_ms = MSEC_TO_UNITS(m_adv_params.min_conn_interval_ms, UNIT_1_25_MS);
  max_conn_interval_ms = MSEC_TO_UNITS(m_adv_params.max_conn_interval_ms, UNIT_1_25_MS);
  
  // устанавливаю параметры после установления соединения (внешнее подключение, если разрешено)
  ERROR_CHECK(gap_params_init(m_adv_params.device_name, min_conn_interval_ms, max_conn_interval_ms));
  
  // устанавливаю новые параметры и запускаю эдвертайзинг

  ble_advdata_manuf_data_t manuf_specific_data = {0}; // структура для размещения данных производителя
  // заполняю данные производителя
  manuf_specific_data.company_identifier = APP_COMPANY_IDENTIFIER; // идентификатор фирмы-изготовителя 
  manuf_specific_data.data.p_data = (uint8_t *)manuf_data; // указатель на структуру данных
  manuf_specific_data.data.size = manuf_data_len;

  // Build and set advertising data.
  memset(&advdata, 0, sizeof(advdata));
    
  advdata.name_type             = BLE_ADVDATA_FULL_NAME;
  advdata.flags                 = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE; // эдвертайзинг с возможностью подключения
  advdata.p_manuf_specific_data = &manuf_specific_data;
  advdata.include_appearance    = true;
        
  ERROR_CHECK(ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len));
  
  // Build and set scan response data.
  // прикручиваю Universally unique service identifier в scan responce data (эти данные будут переданы только по запросу от центрального устройства)
  // эти данные могут быть получены центральным устройством при активном сканировании
  memset(&srdata, 0, sizeof(srdata));
  srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  srdata.uuids_complete.p_uuids  = m_adv_uuids;

  ERROR_CHECK(ble_advdata_encode(&srdata, m_adv_data.scan_rsp_data.p_data, &m_adv_data.scan_rsp_data.len));
  
  // Initialize advertising parameters (used when starting advertising).
  memset(&m_gap_adv_params, 0, sizeof(m_gap_adv_params));

  m_gap_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;  // Эдвертайзинг для всех

  m_gap_adv_params.p_peer_addr     = NULL;    // Undirected advertisement.
  m_gap_adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY; // возможно подключение любых устройств, а не только из белого списка
  m_gap_adv_params.interval        = interval_ms; // интервал между пакетами эдвертайзинга
  m_gap_adv_params.duration        = duration_ms; // длительность эдвертайзинга

  ERROR_CHECK(sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &m_gap_adv_params));
    
  if(m_adv_params.conn_en)
  { // ПОДКЛЮЧЕНИЯ РАЗРЕШЕНЫ
    ERROR_CHECK(sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG));
    RTT_LOG_DEBUG("BLE: Advertising started");
  }else{
    // ПОДКЛЮЧЕНИЯ ЗАПРЕЩЕНЫ
    // Initialize advertising parameters (used when starting advertising).
    memset(&m_gap_adv_params, 0, sizeof(m_gap_adv_params));

    m_gap_adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;  // Эдвертайзинг для всех
    m_gap_adv_params.p_peer_addr     = NULL;    // Undirected advertisement.
    m_gap_adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY; // возможно подключение любых устройств, а не только из белого списка
    m_gap_adv_params.interval        = interval_ms; // интервал между пакетами эдвертайзинга
    m_gap_adv_params.duration        = duration_ms; // длительность эдвертайзинга

    ERROR_CHECK(sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &m_gap_adv_params));

    ERROR_CHECK(sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG));
    RTT_LOG_DEBUG("BLE: Advertising no connection started...");
  }
  
  // выставляю выходную мощность в процессе эдвертайзинга
  sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, (int8_t)m_adv_params.pwrAdv);
  return NRF_SUCCESS;
}


/*
* Принудительная остановка эдвертайзинга
*/
void bleAdvStop(void)
{
  RTT_LOG_DEBUG("BLE: Advertising stopped");
  sd_ble_gap_adv_stop(m_adv_handle);
}


/*
* Обновление данных производителя в процессе эдвертайзинга
* manuf_data - данные производителя
* manuf_data_len - длина данных производителя
* возвращает код ошибки из nrf_errors.h
*/
ret_code_t bleAdvManufDataUpdate(void *manuf_data, uint8_t manuf_data_len)
{
  VERIFY_PARAM_NOT_NULL(manuf_data);
  // перезапускаю эдвертайзинг с новыми данными производителя
  // имеют значение только новые парамерты данных производителя, остальные - не используются
  return bleAdvStart(NULL, 0, 0, BLE_PWR_0, 0, 0, BLE_PWR_0, manuf_data, manuf_data_len, false);  
}


/*
* Инициализация сервиса информации об устройстве (сервис DIS)
* Если сервис не будет инициализирован, то в полях будет всякий мусор
* Данные доступны для просмотра только после установления соединения
* manuf_name - название фирмы производителя
* fw_ver - версия ПО
* hw_ver - версия железа
* manuf_id - ID производителя 64 бита
* org_unique_id - уникальный ID производителя 32 бита
* возвращает код ошибки nrf_errors.h
* если строки на входе слишком длинные, то будет ошибка 
*/
ret_code_t bleDeviceInfoInit(const char *manuf_name, const char *fw_ver, const char *hw_ver, uint64_t manuf_id, uint32_t org_unique_id)
{ // инициализация сервиса информации об устройстве
  ble_dis_init_t     dis_init;
  
  // Initialize Device Information Service.
  memset(&dis_init, 0, sizeof(dis_init));

  ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, (char *)manuf_name);
  char serial_num[17];
  uint64_t dev_id = (*((uint64_t*) NRF_FICR->DEVICEADDR));
  snprintf(serial_num, sizeof(serial_num), "%04X%04X", (uint32_t)(dev_id >> 32), (uint32_t)dev_id);
  ble_srv_ascii_to_utf8(&dis_init.serial_num_str, (char *)serial_num);
  ble_srv_ascii_to_utf8(&dis_init.fw_rev_str, (char *)fw_ver);
  ble_srv_ascii_to_utf8(&dis_init.hw_rev_str, (char *)hw_ver);
    
  ble_dis_sys_id_t system_id;
  system_id.manufacturer_id            = manuf_id;
  system_id.organizationally_unique_id = org_unique_id;
  dis_init.p_sys_id                    = &system_id;

  dis_init.dis_char_rd_sec = SEC_OPEN;    

  return ble_dis_init(&dis_init);
}


/*
* Подтверждение установления аутентификации при установлении защищенного соединения
*/
void bleAuthKeyReply(uint16_t conn_handle)
{
  sd_ble_gap_auth_key_reply(conn_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, NULL);
}


/*
* Разрыв соединения
* conn_handle - ID подключения
* возвращает код ошибки nrf_errors.h
*/
ret_code_t bleDisconnect(uint16_t conn_handle)
{
  return sd_ble_gap_disconnect(conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
}


/*
* Управление парингом (разрешение/запрещение)
* en - если true, значит паринг разрешен
*/
void bleParingEn(bool en)
{
  m_paring_en = en;
}


/*
* Удаление одного или нескольких ранее спаренных устройств
* peerID - номер устройства в таблице (если -1, то будут удалены все устройства)
* возвращает код ошибки из nrf_errors.h
*/
ret_code_t bleDeletePeers(int16_t peerID)
{
  // TODO добавить отправку информации об удаляемом пире на верхний уровень
  if(peerID < 0)
    return pm_peers_delete();
  return pm_peer_delete(peerID);
}


/*
* Запрос наличия спаренных устройтсв
* возвращает количество спаренных устройств
*/
uint16_t bleGetPeerCnt(void)
{
  return pm_peer_count();
}

/*
* Запрос ID следующего пира после заданного
* peerID - текущий peerID
* возвращает ID следующего пира или PM_PEER_ID_INVALID
*/
uint16_t bleGetNextPeerID(uint16_t peerID)
{
  return pm_next_peer_id_get(peerID);
}


/*
* Установка указателей на примный и передающий буферы
* conn_handle - ID установленного соединения
* rx_stream_buff_handle - хендл приемного буфера
* tx_stream_buff_handle - хендл передающего потокового буфера
* возвращает код ошибки из ntr_errors.h
*/
ret_code_t bleSetNusBuffers(uint16_t conn_handle, StreamBufferHandle_t rx_stream_buff_handle, StreamBufferHandle_t tx_stream_buff_handle)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return NRF_ERROR_CONN_COUNT;
  
  m_connected_peers[conn_handle].rx_stream_buff_handle = rx_stream_buff_handle;
  m_connected_peers[conn_handle].tx_stream_buff_handle = tx_stream_buff_handle;
  
  return NRF_SUCCESS;
}


/*
* Отправка данных через NUS
* conn_handle - ID соединения
* p_data - указатель на буфер с данными для передачи
* data_size - размер даных для передачи
* sema - семафор окончания передачи
* возвращает код ошибки из nrf_errors.h
*/
ret_code_t bleNusTx(uint16_t conn_handle, void *p_data, uint32_t data_size, SemaphoreHandle_t sema)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return NRF_ERROR_CONN_COUNT;
  
  if(m_connected_peers[conn_handle].tx_stream_buff_handle == NULL) return NRF_ERROR_INVALID_ADDR;
  
  xTaskNotifyGive(m_nus_tx_thread); // отправляю нотификатор для старта процесса передачи
  
  if(xStreamBufferSpacesAvailable(m_connected_peers[conn_handle].tx_stream_buff_handle) < data_size) return NRF_ERROR_NO_MEM;
  
  xStreamBufferSend(m_connected_peers[conn_handle].tx_stream_buff_handle, p_data, data_size, 0);
  m_connected_peers[conn_handle].tx_done_sema = sema;
  
  return NRF_SUCCESS;
}


/*
* Отправка данных через NUS с ожиданием, пока освободится передающий буфер, если данные не помещаются
* conn_handle - ID соединения
* p_data - указатель на буфер с данными для передачи
* data_size - размер даных для передачи
* wait_ms - максимальное время ожидания размещения данных в буфере
* возвращает код ошибки из nrf_errors.h
*/
ret_code_t bleNusTxWait(uint16_t conn_handle, void *p_data, uint32_t data_size, uint32_t wait_ms)
{
  if(conn_handle >= NRF_BLE_LINK_COUNT) return NRF_ERROR_CONN_COUNT;
  
  if(m_connected_peers[conn_handle].tx_stream_buff_handle == NULL) return NRF_ERROR_INVALID_ADDR;
  
  xTaskNotifyGive(m_nus_tx_thread); // отправляю нотификатор для старта процесса передачи
  m_connected_peers[conn_handle].tx_done_sema = NULL;
  if(data_size != xStreamBufferSend(m_connected_peers[conn_handle].tx_stream_buff_handle, p_data, data_size, pdMS_TO_TICKS(wait_ms))) return NRF_ERROR_TIMEOUT;
  
  return NRF_SUCCESS;
}


/*
 * Обновление информации о заряде батареи
 * battery_level - уровень заряда батареи
 * возвращает ошибку из nrf_error.h
 */
ret_code_t bleBatteryServiceUpdate(uint8_t battery_level)
{
  ret_code_t err_code;

  err_code = ble_bas_battery_level_update(&m_bas, battery_level, BLE_CONN_HANDLE_ALL);
  if ((err_code != NRF_SUCCESS) &&
      (err_code != NRF_ERROR_INVALID_STATE) &&
      (err_code != NRF_ERROR_RESOURCES) &&
      (err_code != NRF_ERROR_BUSY) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
       )
  {
    return(err_code);
  }
	return NRF_SUCCESS;
}


/*
* Получение статистики по использованию флеша (FDS) для дебага
* stat - статистика
* Возвращает ошибку из nrf_errors.h
*/
uint16_t blePrintFlashStats(ble_fds_stat_t *stat)
{
  return fds_stat((fds_stat_t *)stat);
}


/*
* Очистка FDS от ранее удаленных записей
* Возвращает ошибку из nrf_errors.h
*/
uint16_t bleGarbageCollector(void)
{
  return fds_gc();
}

