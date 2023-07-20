#ifndef ADS129X_H
#define ADS129X_H

/**
 * Структуры и функции для работы с АЦП ADS129x
 * 
 * 
 * ОСОБЕННОСТИ
 * - функции блокируют поток до окончания их выполнения
*/

#include <stdbool.h>
#include <stdint.h>

#define ADS129X_CH_CNT        8       ///< Количество каналов АЦП

/// Значения команд
#define ADS129X_CMD_WAKEUP    0x02    ///< Пробуждение из режима пониженного потребления
#define ADS129X_CMD_STANDBY   0x04    ///< Переход в режим пониженного потребления
#define ADS129X_CMD_RESET     0x06    ///< Сброс устройства
#define ADS129X_CMD_START     0x08    ///< Запуск/перезапуск измерений
#define ADS129X_CMD_STOP      0x0A    ///< Остановка измерений
#define ADS129X_CMD_RDATAC    0x10    ///< Включение режима непрерывных измерений
#define ADS129X_CMD_SDATAC    0x11    ///< Выключение режима непрерывных измерений
#define ADS129X_CMD_RDATA     0x12    ///< Команда чтения данных
#define ADS129X_CMD_RREG      0x20    ///< Чтение регистров
#define ADS129X_CMD_WREG      0x40    ///< Запись регистров


typedef uint32_t *ads129x_handle_t;

typedef struct {
  uint8_t  val[3];
}ads129x_24bit_t;


// формат данных от АЦП: 24 бита статус и 8 каналов по 24 бита данные (всего 216 бит)
typedef struct {
  ads129x_24bit_t   status;
  ads129x_24bit_t   ch[ADS129X_CH_CNT];
} ads129x_data_t;


/**
 * @brief Добавление устройства на шину SPI
 *
 * @param spiDevID - ID интерфейса SPI (получается после инициализации SPI)
 * @param csPin - номер выхода чип-селекта (если не используется, то SPI_PIN_NOT_USED)
 * @param handle - хендл устройства на шине SPI
 * 
 * @return
 *  ERR_NOERROR: ошибок нет
 *  ERR_OUT_OF_MEMORY: ошибка при выделении памяти
*/
uint16_t ads129x_add(uint8_t spiDevID, uint8_t csPin, ads129x_handle_t *handle);


/**
 * @brief Удаление устройства с шины SPI
 *
 * @param handle - хендл устройства на шине SPI
*/
void ads129x_remove(ads129x_handle_t handle);


/**
 * @brief Отправка команды
 *
 * @param handle - хендл устройства на шине SPI
 * @param cmd - код команды
 * @param timeout_ms - таймаут ожидания доступа к интерфейсу
 * 
 * @return
 *  ERR_NOERROR: ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads129x_cmd(ads129x_handle_t handle, uint8_t cmd, uint32_t timeout_ms);


/**
 * @brief Чтение регистров
 *
 * @param handle - хендл устройства на шине SPI
 * @param reg_addr - адрсе регистра
 * @param cnt - количество регистров для чтения
 * @param rx_data - указатель на буфер
 * @param timeout_ms - таймаут ожидания доступа к интерфейсу
 * 
 * @return
 *  ERR_NOERROR: ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads129x_read_regs(ads129x_handle_t handle, uint8_t reg_addr, uint8_t cnt, uint8_t *rx_data, uint32_t timeout_ms);


/**
 * @brief Запись регистров
 *
 * @param handle - хендл устройства на шине SPI
 * @param reg_addr - адрсе регистра
 * @param cnt - количество регистров для записи
 * @param tx_data - указатель на буфер
 * @param timeout_ms - таймаут ожидания доступа к интерфейсу
 * 
 * @return
 *  ERR_NOERROR: ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads129x_write_regs(ads129x_handle_t handle, uint8_t reg_addr, uint8_t cnt, uint8_t *tx_data, uint32_t timeout_ms);

/**
 * @brief Запись регистра
 *
 * @param handle - хендл устройства на шине SPI
 * @param reg_addr - адрсе регистра
 * @param val - значение для записи
 * @param timeout_ms - таймаут ожидания доступа к интерфейсу
 * 
 * @return
 *  ERR_NOERROR: ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads129x_write_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t val, uint32_t timeout_ms);

/**
 * @brief Чтение данных (формат данных описан в документации на ads129x)
 * 
 * Данные и статус имеют 24 разряда
 *
 * @param handle - хендл устройства на шине SPI
 * @param rx_data - указатель на буфер данных
 * @param timeout_ms - таймаут ожидания доступа к интерфейсу
 * 
 * @return
 *  ERR_NOERROR: ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads129x_read_data(ads129x_handle_t handle, ads129x_data_t *rx_data, uint32_t timeout_ms);




#endif
