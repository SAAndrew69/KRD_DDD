#ifndef BLEDRIVER_H
#define BLEDRIVER_H

/*
Драйвер для работы с BLE. Может поддерживать несколько входящих и исходящих соединение одновремменно
*/

#include <stdbool.h>
#include <stdint.h>
#include "sdk_errors.h"
#include "sdk_config.h"
#include "peer_manager_types.h"

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "semphr.h"


/// @brief Максимальное количество линков входящих соединений
#define NRF_BLE_LINK_COUNT    NRF_SDH_BLE_PERIPHERAL_LINK_COUNT // задаются в sdk_config.h

/// @brief Тип сообщения
typedef enum
{
  BLE_EVENT_TYPE_ADV_REPORT,  ///< принят очередной пакет из рассылки
  BLE_EVENT_TYPE_NUS_RX,      ///< приняты данные через NUS
  BLE_EVENT_TYPE_CONNECT,     ///< случилось подключение (авторизация прошла успешно)
  BLE_EVENT_TYPE_DISCONNECT,  ///< случилось отключение
  BLE_EVENT_TYPE_GAP,         ///< различные эвенты от GAP (enum BLE_GAP_EVTS)
  BLE_EVENT_TYPE_PASSWORD,    ///< передача пароля для сопряжения на верхний уровень
  BLE_EVENT_TYPE_NEW_BONDING, ///< сообщение о новом бондинге
} bleEvtType_t;

/// @brief Тип источника сообщения
typedef enum
{
  BLE_EVENT_SRC_NONE,         ///< источник данных не определен
  BLE_EVENT_SRC_PERIPHERAL,   ///< роль перефирийного устройства (входящее соединение)
} bleEvtSrc_t;

/// @brief Данные производителя из эдвертайзинга
typedef struct
{
  uint8_t               addr[BLE_GAP_ADDR_LEN];     ///< адрес
  uint8_t               data[NRF_BLE_SCAN_BUFFER];  ///< максимальная длина пакета с данными
  uint8_t               dataLen;
} bleAdvManufData_t;

/// @brief Значения выходной мощности
typedef enum
{ // значения выходной мощности (-40dBm, -20dBm, -16dBm, -12dBm, -8dBm, -4dBm, 0dBm, +2dBm, +3dBm, +4dBm, +5dBm, +6dBm, +7dBm and +8dBm)
  BLE_PWR_m40   =(-40),
  BLE_PWR_m20   =(-20),
  BLE_PWR_m16   =(-16),
  BLE_PWR_m12   =(-12),
  BLE_PWR_m8    =(-8),
  BLE_PWR_0     =0,
  BLE_PWR_2     =2,
  BLE_PWR_3     =3,
  BLE_PWR_4     =4,
  BLE_PWR_5     =5,
  BLE_PWR_6     =6,
  BLE_PWR_7     =7,
  BLE_PWR_8     =8
} blePwr_t;

/// @brief Структура запроса пароля при установлении защищенного соединения
typedef struct
{
  char          *spasskey;      ///< указатель на цифровой ключ (6 символов, 0..9), который нужно ввести на внешнем устройстве при паринге
  bool          match_request;  ///< требуется подтверждение пароля         
} blePswReq_t;

/// @brief Структура данных для mainCallback
typedef struct
{
  bleEvtSrc_t           evtSrc;       ///< источник сообщения
  bleEvtType_t          evtType;      ///< тип сообщения
  uint16_t              conn_handle;  ///< номер соединения
  ///< дополнительные данные (зависят от типа сообщения)
  union
  {
    uint16_t                gapEvt;     ///< различные эвенты GAP
    bleAdvManufData_t       manufData;  ///< данные производителя обнаруженного при сканировании устройства
    blePswReq_t             pswReq;     ///< авторизация при паринге и бондинге
  }evt;
} bleCallback_t;

/// @brief Статистика файловой системы (используется peer manager)
typedef struct
{
    uint16_t pages_available;   ///< The number of pages available.
    uint16_t open_records;      ///< The number of open records.
    uint16_t valid_records;     ///< The number of valid records.
    uint16_t dirty_records;     ///< The number of deleted ("dirty") records.
    uint16_t words_reserved;    ///< The number of words reserved by @ref fds_reserve().

    /**@brief The number of words written to flash, including those reserved for future writes. */
    uint16_t words_used;

    /**@brief The largest number of free contiguous words in the file system.
     *
     * This number indicates the largest record that can be stored by FDS.
     * It takes into account all reservations for future writes.
     */
    uint16_t largest_contig;

    /**@brief The largest number of words that can be reclaimed by garbage collection.
     *
     * The actual amount of space freed by garbage collection might be less than this value if
     * records are open while garbage collection is run.
     */
    uint16_t freeable_words;

    /**@brief Filesystem corruption has been detected.
     *
     * One or more corrupted records were detected. FDS will heal the filesystem automatically
     * next time garbage collection is run, but some data may be lost.
     *
     * @note: This flag is unrelated to CRC failures.
     */
    bool corruption;
} ble_fds_stat_t;


/// @brief Тип функции колбэка
typedef void (*bleDriverCallback_t)(bleCallback_t *args);


/**
 * @brief Начальная инициализация драйвера: инициализирует сервисы, приемный и передающий буферы
 * 
 * @param callback - колбэк для дополнительных обработчиков различных событий
 * @param useDefaultPass - если true, то при сопряжении будет использоваться дефолтный пароль
 * @return
 *  код ошибки из nrf_errors.h
 
*/
ret_code_t bleInit(bleDriverCallback_t callback, bool useDefaultPass);


/**
 * @brief Начальная инициализация Peer manager
 * 
 * Требуется для резервирования памяти порядка 6к из кучи, при запуске из задачи выдает ошибку)
 * Можно выполнить до начальной инициализации bleInit()
 * @return
 *  код ошибки из nrf_errors.h
*/
ret_code_t blePMInit(void);


/**
 * @brief Запуск эдвертайзинга
 * 
 * @param device_name - имя устройства в процессе эдвертайзинга
 * @param interval_ms - интервал сканирования в мс
 * @param duration_ms - длительность сканирования в мс (если =0, значит бессрочно)
 * @param pwrAdv - мощность в процессе эдвертайзинга
 * @param min_conn_interval_ms - минимальный интервал после подключения
 * @param max_conn_interval_ms - максимальный интервал после подключения
 * @param pwrConn - мощность после установления соединения
 * @param manuf_data - указатель на данных производителя (есть ограничение по длине!)
 * @param manuf_data_len - длина данных производителя
 * @param conn_en - флаг разрешения подключения
 * @return 
 *  код ошибки из nrf_errors.h
*/
ret_code_t bleAdvStart(const char *device_name, uint32_t interval_ms, uint32_t duration_ms, blePwr_t pwrAdv, uint32_t min_conn_interval_ms, uint32_t max_conn_interval_ms, blePwr_t pwrConn, 
                        void *manuf_data, uint8_t manuf_data_len, bool conn_en);


/*
* Принудительная остановка эдвертайзинга
*/
void bleAdvStop(void);


/**
 * @brief Обновление данных производителя в процессе эдвертайзинга
 * 
 * @param manuf_data - данные производителя
 * @param manuf_data_len - длина данных производителя
 * @return 
 *  код ошибки из nrf_errors.h
*/
ret_code_t bleAdvManufDataUpdate(void *manuf_data, uint8_t manuf_data_len);


/**
 * @brief Инициализация сервиса информации об устройстве (сервис DIS)
 * 
 * Если сервис не будет инициализирован, то в полях будет всякий мусор
 * Данные доступны для просмотра только после установления соединения
 * Если строки на входе слишком длинные, то будет ошибка 
 * 
 * @param manuf_name - название фирмы производителя
 * @param fw_ver - версия ПО
 * @param hw_ver - версия железа
 * @param manuf_id - ID производителя 64 бита
 * @param org_unique_id - уникальный ID производителя 32 бита
 * @return
 *  код ошибки nrf_errors.h
*/
ret_code_t bleDeviceInfoInit(const char *manuf_name, const char *fw_ver, const char *hw_ver, uint64_t manuf_id, uint32_t org_unique_id); // инициализация сервиса информации об устройстве


/**
 * @brief Подтверждение установления аутентификации при установлении защищенного соединения
 * 
 * @param conn_handle - ID подключения
*/
void bleAuthKeyReply(uint16_t conn_handle);
  

/**
 * @brief Разрыв соединения
 * 
 * @param conn_handle - ID подключения
 * @return 
 *  код ошибки nrf_errors.h
*/
ret_code_t bleDisconnect(uint16_t conn_handle);


/**
 * @brief Установка указателей на примный и передающий буферы
 * 
 * @param conn_handle - ID установленного соединения
 * @param rx_stream_buff_handle - хендл приемного буфера
 * @param tx_stream_buff_handle - хендл передающего потокового буфера
 * @return 
 *  код ошибки из ntr_errors.h
*/
ret_code_t bleSetNusBuffers(uint16_t conn_handle, StreamBufferHandle_t rx_stream_buff_handle, StreamBufferHandle_t tx_stream_buff_handle);


/**
 * @brief Отправка данных через NUS
 * 
 * @param conn_handle - ID соединения
 * @param p_data - указатель на буфер с данными для передачи
 * @param data_size - размер даных для передачи
 * @param sema - семафор окончания передачи
 * @return
 *  код ошибки из nrf_errors.h
*/
ret_code_t bleNusTx(uint16_t conn_handle, void *p_data, uint32_t data_size, SemaphoreHandle_t sema);


/**
 * @brief Отправка данных через NUS с ожиданием, пока освободится передающий буфер, если данные не помещаются
 * 
 * @param conn_handle - ID соединения
 * @param p_data - указатель на буфер с данными для передачи
 * @param data_size - размер даных для передачи
 * @param wait_ms - максимальное время ожидания размещения данных в буфере
 * @return
 *  код ошибки из nrf_errors.h
*/
ret_code_t bleNusTxWait(uint16_t conn_handle, void *p_data, uint32_t data_size, uint32_t wait_ms);


/**
 * @brief Обновление информации о заряде батареи
 * 
 * @param battery_level - уровень заряда батареи в процентах
 * @return 
 *  код ошибки из nrf_error.h
 */
ret_code_t bleBatteryServiceUpdate(uint8_t battery_level);


/**
 * @brief Управление парингом (разрешение/запрещение)
 * @param en - если true, значит паринг разрешен
*/
void bleParingEn(bool en);


/**
 * @brief Удаление одного или нескольких ранее спаренных устройств
 * 
 * @param peerID - номер устройства в таблице (если -1, то будут удалены все устройства)
 * @return
 *  код ошибки из nrf_errors.h
*/
ret_code_t bleDeletePeers(int16_t peerID);


/**
 * @brief Запрос наличия спаренных устройтсв
 * 
 * @return 
 *  число спаренных устройств
*/
uint16_t bleGetPeerCnt(void);


/**
 * @brief Запрос ID следующего пира после заданного
 * 
 * @param peerID - текущий peerID
 * @return
 *  ID следующего пира или PM_PEER_ID_INVALID
*/
uint16_t bleGetNextPeerID(uint16_t peerID);


/**
 * @brief Получение статистики по использованию флеша (FDS) для дебага
 * @param stat - статистика
 * @return
 *  ошибка из nrf_errors.h
*/
uint16_t blePrintFlashStats(ble_fds_stat_t *stat);


/**
 * @brief Очистка FDS от ранее удаленных записей
 * 
 * @return
 *  Возвращает ошибку из nrf_errors.h
*/
uint16_t bleGarbageCollector(void);
  

#endif
