
/**

*/

#include "settings.h"
#include "custom_board.h"
#include "sys.h"
//#include "utils.h"
#include "spim_freertos.h"
#include "ads129x.h"

#include <stdint.h>
#include <string.h>
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
#include "timers.h"
#include "stream_buffer.h"
#include "message_buffer.h"

#include "math.h"
#include "errors.h"


// НАСТРОЙКИ МОДУЛЯ ************************************

// программирую напряжение питания GPIO в 3.3V (по адресу 0x10001304 будет записано значение UICR_REGOUT0_VOUT_3V3)
const uint32_t UICR_REGOUT0 __attribute__((at(0x10001304))) __attribute__((used)) = UICR_REGOUT0_VOUT_3V3; 


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

} superMsg_id_t;
  
  
typedef struct
{ // структура сообщения для суперзадачи
  superMsg_id_t       msgID;      // ID сообщения
  uint16_t            msgLen;     // длина сообщения
  uint8_t             msg[1];     // сообщение произвольной длины
} superMsg_t;
  
  
static TaskHandle_t           m_superTask = NULL; // хендл суперзадачи для реализации всей логики работы  
static MessageBufferHandle_t  m_superMsgHandle = NULL; // хендл буфера сообщений для суперзадачи  


// #############################  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ  ##############################################
static void ads_task_callback(void *args)
{ // в эту функцию прилетают данные от двух АЦП в формате adstask_data_t

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
  uint8_t       msg_buff[SUPERTASK_MSG_MAX]; // буфер для хранения очередного сообщения (может использовать для других целей в рамках обработки текущего сообщения)
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
        uint16_t err;
        
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
        m_superMsgHandle = xMessageBufferCreate(SUPERTASK_MSG_BUFF_SIZE);
        if(m_superMsgHandle == NULL)
        {
          RTT_LOG_INFO("MAIN: Memory not enough for supertask message buffer");
          state = STATE_NONE;
          break;
        }
        
   
        // НАСТРОЙКА ПЕРЕФИРИИ
        

        // ЗАПУСКАЮ ПРОЦЕССЫ
        err = ads_task_init(ads_task_callback);
        if(err != ERR_NOERROR) 
        {
          RTT_LOG_INFO("MAIN: Init ADS TASK error 0x%02X", err);
          state = STATE_NONE;
          break;
        }
    
        
        RTT_LOG_INFO("MAIN: ******* Startup complete ********");
        
        state = STATE_MAIN;
      }
      break;
// STATE_STARTUP >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      case STATE_MAIN: // ***** РЕАЛИЗАЦИЯ ФУНКЦИОНАЛА УСТРОЙСТВА *****
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      {
        superMsg_t * p_msg = (superMsg_t *)msg_buff; // указатель на сообщение
        // жду очередное сообщение
        uint32_t msg_len = xMessageBufferReceive(m_superMsgHandle, (void *)msg_buff, sizeof(msg_buff), portMAX_DELAY);
        
        switch(p_msg->msgID)
        {
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************          
          default:
            RTT_LOG_INFO("MAIN: UNKNOWN MSG");
            break;
        }
      }
      break;
// STATE_MAIN >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    
      
      default:
        RTT_LOG_INFO(">>>>>>> MAIN: SUPERTASK ERROR! REBOOTING... <<<<<<<");
        vTaskDelay(pdMS_TO_TICKS(20000));
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

// ####################################################################################################


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
    
    // Activate deep sleep mode.
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    // Start FreeRTOS scheduler.
    vTaskStartScheduler();

    for (;;)
    {
       APP_ERROR_HANDLER(NRF_ERROR_FORBIDDEN);
    }
}

