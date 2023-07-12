#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>


/*
Основные настройки модулей программы



ИЗМЕНЕНИЯ



*/
#define RTTLOG_EN										1					// если =1, то разрешен вывод в консоль (энергосбережения в этом режиме НЕТ!)

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
#define VER_SHOW			              "придумать"	// отображаемая версия прошивки
#define DEVICE_NAME 	              "имя устройства" // используется при эдвертайзинге
// ******************************************************
#define BOARD_V1_0									1					// текущая версия платы
// ******************************************************
// ******************************************************
#define BLE_EN											0					// BLE
#define BLE_PARING_DISABLED					0					// если =1, до будет невозможно подключиться к часам как к перефирийному устройству без бондинга						
#define BLE_NO_START								0					// =1, если BLE запускать не нужно (используется при отладке)
#define BLE_AUTH_OFF                1         // =1, если нужно отключить разрыв соединения при ошибке аутентификации (используется при отладке)

#define WDT_EN											0					// модуль вотчдога

// НАСТРОЙКИ МОДУЛЕЙ ###############################

// ******** SUPERTASK *********
#define SUPERTASK_STACK_SIZE				1024			// размер стека суперзадачи (стек выделяется в словах uint32_t)
#define SUPERTASK_PRIORITY					2					// приоритет суперзадачи (TODO возможно, что стоит увеличить приоритет всех остальных задач на 1)
#define SUPERTASK_MSG_BUFF_SIZE			4096			// размер буфера сообщений для суперзадачи в байтах
#define SUPERTASK_MSG_MAX						512				// максимальный размер одного сообщения (если сообщение будет длинее, то оно будет потеряно)

// ******** LOGGER RTT ******** 
#define LOGGER_PRIORITY 						1					// приоритет задачи
#define LOGGER_STACK_SIZE 					512				// The size of the stack for the Logger task (in 32-bit words)

// ******** SPIM **************
#define SPIM_TASK_TIMEOUT_MS      1000  // максимальное время выполнения одной задачи по SPI (при передаче больших объемов на маленькой скорости надо будет увеличивать)

// ******** ADS TASK **********
#define ADSTASK_STACK_SIZE				1024			// размер стека (стек выделяется в словах uint32_t)
#define ADSTASK_PRIORITY					3					// приоритет 
#define ADSTASK_DATA_QUEUE_SIZE             3           // длина очереди принятых данных в блоках данных
#define ADSTASK_CMD_QUEUE_SIZE             5           // длина очереди управляющих команд


// ******** WDT ***************
#define WDT_TIME_CYCLE_MS						30000			// время срабатывания WDT-таймера


#endif