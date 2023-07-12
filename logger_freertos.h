#ifndef LOGGER_FREERTOS_H
#define LOGGER_FREERTOS_H

#include <stdbool.h>
#include <stdint.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/*
Модуль для вывода сообщений в дебажную консоль Segger RTT Viewer
После инициализации можно использовать стандартные макросы:
NRF_LOG_INFO()
NRF_LOG_DEBUG()
и другие
*/


uint16_t log_init(void); // инициализация лога через RTT Viewer





#endif
