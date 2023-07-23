#ifndef ADS_TASK_H
#define ADS_TASK_H

/**
 * Модуль реализует логику работы с двумя АЦП
*/


#include <stdbool.h>
#include <stdint.h>
#include "ads129x.h"
#include "ads1299.h"


/// формат выходных данных от двух АЦП
typedef struct {
  uint32_t   adc0_status;
  uint32_t   adc1_status;
  int32_t    adc0[ADS129X_CH_CNT];
  int32_t    adc1[ADS129X_CH_CNT];
} adstask_data_t;

typedef enum {
  ADSTASK_ADC_MASTER = 0,
  ADSTASK_ADC_SLAVE = 1
} adstask_adc_no_e;


typedef void (*ads_task_callback_t)(adstask_data_t *args);





/**
 * @brief Начальная инициализация модуля
 * 
 * @param callback - Адрес функции обратного вызова
 * 
 * @return 
 *  ERR_NOERROR - если ошибок нет
 *  ERR_OUT_OF_MEMORY - недостаточно памяти
*/
uint16_t ads_task_init(ads_task_callback_t callback);


/**
 * @brief Деинициализация модуля
 * 
*/
uint16_t ads_task_deinit(void);


/**
 * @brief Запуск измерений
 * 
 * @param single_shot - флаг запуска одиночного измерения
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
*/
uint16_t ads_task_start(bool single_shot);


/**
 * @brief Останов измерений
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
*/
uint16_t ads_task_stop(void);


/**
 * @brief Запрос конфига
 * 
 * @param adc_no - номер АЦП
 * @param cfg_srt - строка конфига в формате: n,rrvv,....,rrvv,0 где n - номер АЦП (0 или 1), rrvv - uint16_t, где rr - адрес регистра, vv - значение регистра
 * @param cfg_len_max - максимальный размер буфер под строку с конфигом
 * @param timeout_ms - максимальное время ожидания начала выполенния задания
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
 *  ERR_INVALID_PARAMETR - ошибка входных данных
 *  ERR_TIMEOUT - таймаут ожидания доступа
 *  ERR_INVALID_STATE - АЦП в процессе измерения
*/
uint16_t ads_task_get_config(adstask_adc_no_e adc_no, char *cfg_str, uint8_t cfg_len_max, uint32_t timeout_ms);




#endif
