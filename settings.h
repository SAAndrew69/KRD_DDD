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
#define DEVICE_NAME 	              "CARDIOGLB_18" // используется при эдвертайзинге
// ******************************************************
#define BOARD_V1_0									1					// текущая версия платы
// ******************************************************
#define MANUFACTURER_NAME           "NONAME LTD"      /**< Manufacturer. Will be passed to Device Information Service. */
#define MANUFACTURER_ID             0x55AA55AA55      /**< DUMMY Manufacturer ID. Will be passed to Device Information Service. You shall use the ID for your Company*/
#define ORG_UNIQUE_ID               0xEEBBEE          /**< DUMMY Organisation Unique ID. Will be passed to Device Information Service. You shall use the Organisation Unique ID relevant for your Company */
#define FW_VER                      "0.1.0"            // версия ПО
#if (BOARD_V1_0 == 1)
#define HW_VER                      "0.1.0"            // версия железа
#endif
// ******************************************************
#define BLE_EN											1					// BLE
//#define BLE_PARING_DISABLED					0					// если =1, то будет невозможно подключиться к устройству без бондинга						
#define BLE_NO_ADV								0					// =1, адвертайзинг не будет запущен (используется при отладке)
//#define BLE_AUTH_OFF                1         // =1, если нужно отключить разрыв соединения при ошибке аутентификации (используется при отладке)

#define WDT_EN											0					// модуль вотчдога

#define ADS129X_EN                  1                  // если =1, разрешена работа внешнего АЦП

// НАСТРОЙКИ МОДУЛЕЙ ###############################

// ******** SUPERTASK *********
#define SUPERTASK_STACK_SIZE				1024			// размер стека суперзадачи (стек выделяется в словах uint32_t)
#define SUPERTASK_PRIORITY					2					// приоритет суперзадачи (TODO возможно, что стоит увеличить приоритет всех остальных задач на 1)
#define SUPERTASK_MSG_BUFF_LEN			10 			  // размер очереди сообщений для суперзадачи
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

// ******** BLE NUS ******** 
#define NUS_RX_SIZE_MAX				      64			  // размер приемного фифо для телефона
#define NUS_TX_SIZE_MAX				      2048			// размер передающего потокового буфера для телефона
#define UNIT_NUS_EVT_QUEUE_SIZE			10				// размер очереди сообщений на верхний уровень

#define NUSTX_PRIORITY							2
#define NUSTX_STACK_SIZE						512				// стек для процесса передачи данных по BLE (нижний уровень драйвера)
#define BLE_TASK_STACK							256				// стек задачи - обработчика сообщений от bleTask (верхний уровень)
#define BLE_TASK_PRIORITY						2					// верхний уровень

#define BLE_ADV_POWER_MAX						BLE_PWR_0	// максимальная выходная мощность при эдвертайзинге blePwr_t
#define BLE_CONN_POWER_MAX					BLE_PWR_4	// максимальная выходная мощность при коннекте blePwr_t
#define TIME_ADV_DURATION_MS		    60000			// Advertising duration
#define TIME_ADV_INTERVAL_MS        100       // периодичность включения эдвертайзинга
#define TIME_CONN_INTERVAL_MIN_MS   10        // минимальный интервал после соединения
#define TIME_CONN_INTERVAL_MAX_MS   30        // максимальный интервал после соединения
#define BLE_ADV_START_DELAY_MS      1000      // таймаут между попытками запуска эдвертайзинга
#define BLE_ADV_ERROR_MAX           10        // максимальное количество ошибок при запуске эдвертайзинга
#define BLE_SEND_TIMEOUT_MS         1000      // максимальное вермя ожидания свободного места в очереди передающего буфера

// ******** CMD ************
#define CMD_LEN_MAX									NUS_RX_SIZE_MAX 			// максимальная длина любых данных, которые могут быть переданы одной командой (вместе со всеми служебными полями)


#endif
