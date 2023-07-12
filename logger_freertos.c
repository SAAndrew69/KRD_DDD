/*
Реализованы функции для вывода дебажной информации в консоль RTT Viewer под FreeRTOS
Для использования необходимо сделать соответствующие настройки в sdk_config.h: разделы nRF_LOG и nRF_Segger_RTT
Если используется UART в других модулях, то нужно сделать настройки для вывода лога через RTT

*/

#include "logger_freertos.h"
#include "sys.h"
#include "settings.h"

/* FreeRTOS related */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "nrf_error.h"


/**
 * The priority of the Logger task.
 */
#ifndef LOGGER_PRIORITY
#define LOGGER_PRIORITY             1
#endif

/**
 * The size of the stack for the Logger task (in 32-bit words).
 * Logger uses sprintf internally so it is a rather stack hungry process.
 */
#ifndef LOGGER_STACK_SIZE
#define LOGGER_STACK_SIZE           512
#endif


#define ERROR_CHECK(ERR_CODE)                           \
    do                                                      \
    {                                                       \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);         \
        if (LOCAL_ERR_CODE != NRF_SUCCESS)                  \
        {                                                   \
            return(LOCAL_ERR_CODE);                         \
        }                                                   \
    } while (0)

    
static TaskHandle_t m_logger_thread;      /**< Logger thread. */


/**@brief A function which is hooked to idle task.
 * @note Idle hook must be enabled in FreeRTOS configuration (configUSE_IDLE_HOOK).
 */   
static void appIdleHook(void)
{
  vTaskResume(m_logger_thread);
}


/**@brief Thread for handling the logger.
 *
 * @details This thread is responsible for processing log entries if logs are deferred.
 *          Thread flushes all log entries and suspends. It is resumed by idle task hook.
 *
 * @param[in]   arg   Pointer used for passing some arbitrary information (context) from the
 *                    osThreadCreate() call to the thread.
 */
static void logger_thread(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (1)
    {
        NRF_LOG_FLUSH();

        vTaskSuspend(NULL); // Suspend myself
    }
}

    

/** @brief Function for initializing the nrf_log module. */
uint16_t log_init(void)
{ // инициализация лога через RTT Viewer
  ret_code_t err_code = NRF_LOG_INIT(NULL);
  ERROR_CHECK(err_code);

  NRF_LOG_DEFAULT_BACKENDS_INIT();
  
  sysSetLoggerAppIdleHook(appIdleHook); // устанавливаю обработчик для vApplicationIdleHook()
  
  // Создаю задачу
  if (pdPASS != xTaskCreate(logger_thread,
                            "LOGGER",
                            LOGGER_STACK_SIZE,
                            NULL,
                            LOGGER_PRIORITY,
                            &m_logger_thread))
  {
      err_code = NRF_ERROR_NO_MEM;
  }
  
  return err_code;
}


























