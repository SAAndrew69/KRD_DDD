/*
Монитор ресурсов запущенных задач. Позволят контролировать использование выделенной задачам памяти, а так же время выполнения задач.
Источник: https://habr.com/ru/post/352782/

ВНИМАНИЕ!
Для работы модуля необходимо в настройках freertos установить параметр configUSE_TRACE_FACILITY в единицу
Для разрешения подсчета времени работы каждой задачи, нужно установить параметр configGENERATE_RUN_TIME_STATS в 1 и добавить в FreeRTOSConfig.h следующие определения:
#if configGENERATE_RUN_TIME_STATS == 1
void vConfigureTimerForRunTimeStats(void);
uint32_t vGetTimerForRunTimeStats(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()    vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()            vGetTimerForRunTimeStats()
#endif // configGENERATE_RUN_TIME_STATS


ОСОБЕННОСТИ
- использует стек той задачи, из которой будет вызываться (лучше сделать отдельную задачу)


ВЫВОДИМАЯ ИНФОРМАЦИЯ О ЗАДАЧЕ
The handle of the task to which the rest of the information in the structure relates.
TaskHandle_t xHandle;

A pointer to the task's name. This value will be invalid if the task was deleted since the structure was populated!
const signed char *pcTaskName;

A number unique to the task.
UBaseType_t xTaskNumber;

The state in which the task existed when the structure was populated.
eTaskState eCurrentState;

The priority at which the task was running (may be inherited) when the structure was populated.
UBaseType_t uxCurrentPriority;

The priority to which the task will return if the task's current priority has been inherited to avoid unbounded priority inversion when obtaining a
mutex. Only valid if configUSE_MUTEXES is defined as 1 in FreeRTOSConfig.h.
UBaseType_t uxBasePriority;

The total run time allocated to the task so far, as defined by the run time stats clock. Only valid when configGENERATE_RUN_TIME_STATS is
defined as 1 in FreeRTOSConfig.h.
unsigned long ulRunTimeCounter;

Points to the lowest address of the task's stack area.
StackType_t *pxStackBase;

The minimum amount of stack space that has remained for the task since the task was created. The closer this value is to zero the closer the task
has come to overflowing its stack.
unsigned short usStackHighWaterMark;


*/


#include "debug_monitor.h"
#include "settings.h"

/* FreeRTOS related */
#include "FreeRTOS.h"
#include "task.h"





// НАСТРОЙКИ МОДУЛЯ ************************************
#ifndef LOGGER_EN
#define LOGGER_EN                 1       // разрешение работы монитора
#endif
// для разрешение работы таймера для подсчета времени нахождения в задачах устновить configGENERATE_RUN_TIME_STATS в FreeRTOSConfig.h в 1

#define TM                        NRF_TIMER4  // используемый таймер
#define DEBUG_MON_TASKS_MAX       30          // максимальное количество задач, информация о которых может быть выведена

// *****************************************************


#if(LOGGER_EN)
#include "logger_freertos.h"
#define RTT_LOG_EN                1       // включить лог через RTT
#else
#define RTT_LOG_EN 0
#endif // LOGGER_EN

#if(RTT_LOG_EN)
#define RTT_LOG_INFO(...)       \
                                \
  {                             \
    NRF_LOG_INFO(__VA_ARGS__);  \
  }
#else
  #define RTT_LOG_INFO(...) {}
#endif // RTT_LOG_EN

  
  
  

#if(LOGGER_EN)

static TaskStatus_t m_buffer[DEBUG_MON_TASKS_MAX]; // буфер для хранения статистики по задачам

#if configGENERATE_RUN_TIME_STATS == 1

void vConfigureTimerForRunTimeStats(void)
{ // инициализация и запуск таймера
  TM->TASKS_STOP = TIMER_TASKS_STOP_TASKS_STOP_Msk;
  TM->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos; // режим таймера
  TM->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos; // 32 бита
  TM->PRESCALER = 4 << TIMER_PRESCALER_PRESCALER_Pos; // коэф. деления 1/4 (клок 16 МГц)
  TM->TASKS_CLEAR = TIMER_TASKS_CLEAR_TASKS_CLEAR_Msk;
  TM->TASKS_START = TIMER_TASKS_START_TASKS_START_Msk;
}

uint32_t vGetTimerForRunTimeStats(void)
  { // возвращает текущее значение таймера (максимальное время мониторинга - немногим больше 70 минут)
  TM->TASKS_CAPTURE[0] = TIMER_TASKS_CAPTURE_TASKS_CAPTURE_Msk;
  return TM->CC[0];
}

#endif // configGENERATE_RUN_TIME_STATS
  
  
  
  
  
static char *_task_state_to_char(eTaskState state)
{ // преобразует статус задачи в строку
  
  switch(state)
  {
    case eRunning:  /* A task is querying the state of itself, so must be running. */
      return "RUNNING";
    
    case eReady:      /* The task being queried is in a read or pending ready list. */
      return "READY";
    
    case eBlocked:    /* The task being queried is in the Blocked state. */
      return "BLOCKED";
    
    case eSuspended:    /* The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
      return "SUSPENDED";
    
    case eDeleted:    /* The task being queried has been deleted, but its TCB has not yet been freed. */
      return "DELETED";
  
    case eInvalid:      /* Used as an 'invalid state' value. */
      return "INVALID";
    
    default:
      break;
  }
  return "UNKNOWN";
}
  



void printTasksStats(void)
{
  UBaseType_t task_count = uxTaskGetNumberOfTasks(); // получаю количество созданных задач
  
  uint32_t _total_runtime;
  
  if(task_count > DEBUG_MON_TASKS_MAX)
  {
    RTT_LOG_INFO("Task count was truncated to DEBUG_MON_TASKS_MAX");
    task_count = DEBUG_MON_TASKS_MAX;
  }

  task_count = uxTaskGetSystemState(m_buffer, task_count, &_total_runtime); // получаю информацию о задачах
  
  // вывожу полученную информацию в консоль
#if configGENERATE_RUN_TIME_STATS == 1
  for (int task = 0; task < task_count; task++)
    {
      RTT_LOG_INFO("MONITOR: %20s: %10s, %u, %6u, %u ms", 
                            m_buffer[task].pcTaskName,
                            _task_state_to_char(m_buffer[task].eCurrentState),
                            m_buffer[task].uxCurrentPriority,
                            m_buffer[task].usStackHighWaterMark,
                            m_buffer[task].ulRunTimeCounter / 1000);
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  RTT_LOG_INFO("MONITOR: Current Heap Free Size: %u", xPortGetFreeHeapSize());

  RTT_LOG_INFO("MONITOR: Minimal Heap Free Size: %u", xPortGetMinimumEverFreeHeapSize());
                                 
  RTT_LOG_INFO("MONITOR: Total RunTime:  %u ms", _total_runtime / 1000);

  RTT_LOG_INFO("MONITOR: System Uptime:  %u ms\r\n", xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
  for (int task = 0; task < task_count; task++)
    {
      RTT_LOG_INFO("MONITOR: %20s: %10s, %u, %6u", 
                            m_buffer[task].pcTaskName,
                            _task_state_to_char(m_buffer[task].eCurrentState),
                            m_buffer[task].uxCurrentPriority,
                            m_buffer[task].usStackHighWaterMark);
    }
  RTT_LOG_INFO("MONITOR: Current Heap Free Size: %u", xPortGetFreeHeapSize());

  RTT_LOG_INFO("MONITOR: Minimal Heap Free Size: %u", xPortGetMinimumEverFreeHeapSize());

  RTT_LOG_INFO("MONITOR: System Uptime:  %u ms\r\n", xTaskGetTickCount() * portTICK_PERIOD_MS);
#endif // configGENERATE_RUN_TIME_STATS
}

#else
void printTasksStats(void) {}
#endif // LOGGER_EN











