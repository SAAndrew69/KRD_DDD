#ifndef ADS_TASK_H
#define ADS_TASK_H

/**
 * Модуль реализует логику работы с двумя АЦП
*/


#include <stdbool.h>
#include <stdint.h>


/// формат выходных данных от двух АЦП
typedef struct {
  ads129x_24bit_t adc0_status;
  ads129x_24bit_t adc1_status;
  ads129x_24bit_t  adc0[ADS129X_CH_CNT];
  ads129x_24bit_t  adc1[ADS129X_CH_CNT];
} adstask_data_t;


typedef void (*ads_task_callback_t)(void *args);





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







#endif