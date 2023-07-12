
#include "sys.h"
#include "settings.h"
#include "string.h"
#include "nrf.h"
#include "custom_board.h"
#include "nrf_power.h"

#if(WDT_EN)
#include "nrf_drv_wdt.h"
#endif // WDT_EN


/*
Здесь объявлены функции-обработчики прерываний, в том числе и некоторый внешние функции FreeRTOS




*/

#define GPIOTE_CH_CNT     8   // максимальное количество каналов GPIOTE



#if(RTTLOG_EN)
static TPTR m_loggerHook = NULL;    // указатель на функцию в logger
#endif

static TPTA m_spim3HookA = NULL; // указатель на функци обработчик прерывания от SPIM3
static void *m_spim3Args = NULL; // указатель на аргументы обработчика прерывания от SPIM3

#if(WDT_EN)
static nrf_drv_wdt_channel_id m_wdt_id; // для обращению к wdt
#endif


/**@brief A function which is hooked to idle task.
 * @note Idle hook must be enabled in FreeRTOS configuration (configUSE_IDLE_HOOK).
 */
void vApplicationIdleHook( void )
{
#if RTTLOG_EN
   if(m_loggerHook)  m_loggerHook();
#endif
}

void GPIOTE_IRQHandler(void)
{ // обработчик прерывания от GPIOTE

} // GPIOTE_IRQHandler


void SPIM3_IRQHandler(void)
{ // обработчик SPIM3
  if(NRF_SPIM3->ENABLE == (SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos))
  {
    if(m_spim3HookA) m_spim3HookA((void *)m_spim3Args);
  }
}


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void sysSetLoggerAppIdleHook(TPTR hook)
{ // установка обработчика от модуля logger в системную функцию vApplicationIdleHook()
#if(RTTLOG_EN)
  m_loggerHook = hook;
#endif // RTTLOG_EN
}

void sysSetSpim3Hook(TPTA hookA, void *args)
{ // установка обработчика от шины SPIM3
  m_spim3HookA = hookA;
  m_spim3Args = args;
}

void systemReset(void)
{ // перезагрузка системы
  __sd_nvic_irq_disable();
  sd_nvic_SystemReset();
}

#if(WDT_EN)
static void wdt_event_handler(void)
{
 // сработка WDT
 // после этого прерывания произойдет сброс
}

uint16_t WDT_Run(uint32_t time)
{ // запуск WDT (time - в мс)
  nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
  config.reload_value = time;
  uint16_t err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
  if(err_code != NRF_SUCCESS) return err_code;
  err_code = nrf_drv_wdt_channel_alloc(&m_wdt_id);
  if(err_code != NRF_SUCCESS) return err_code;
  nrf_drv_wdt_enable();
  return err_code;
}

#endif // WDT_EN

void WDT_Reset(void)
{ // сброс wdt-таймера
#if(WDT_EN)
  nrf_drv_wdt_channel_feed(m_wdt_id);
#endif
}








