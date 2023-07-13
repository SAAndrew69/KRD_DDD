/**
 * Модуль реализует логику работы с двумя АЦП
 * 
 * 11.07.2023 - Начало работы
 * 
 * 
 * ЛОГИКА РАБОТЫ
 * 
 * 
 * СДЕЛАТЬ
 * - прикрутить режим Power Down
 * - добавить возможность работы с АЦП ADS1298
*/

#include "nrf.h"
#include "ads_task.h"
#include "ads129x.h"
#include "ads1299.h"
#include "settings.h"
#include "custom_board.h"
#include "nrf_delay.h"
#include "errors.h"
#include "string.h"
#include "spim_freertos.h"
#include "sys.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"



// НАСТРОЙКИ МОДУЛЯ ************************************
#ifndef ADSTASK_STACK_SIZE
#define ADSTASK_STACK_SIZE				          1024  // размер стека (стек выделяется в словах uint32_t)
#endif // ADSTASK_STACK_SIZE
#ifndef ADSTASK_PRIORITY
#define ADSTASK_PRIORITY					          3			// приоритет 
#endif // ADSTASK_PRIORITY
#ifndef ADSTASK_DATA_QUEUE_SIZE
#define ADSTASK_DATA_QUEUE_SIZE             3     // длина очереди принятых данных в блоках данных
#endif // ADSTASK_DATA_QUEUE_SIZE
#ifndef ADSTASK_CMD_QUEUE_SIZE
#define ADSTASK_CMD_QUEUE_SIZE              5     // длина очереди управляющих команд
#endif // ADSTASK_CMD_QUEUE_SIZE

#if(RTTLOG_EN)
#include "logger_freertos.h"
#define RTT_LOG_EN                1     // включить лог через RTT
#define TAG                     "ADS_TASK: "
#else
#define RTT_LOG_EN 0
#endif // RTTLOG_EN
// *****************************************************

#if(RTT_LOG_EN)
#define RTT_LOG_INFO(...)       \
                                \
  {                             \
    NRF_LOG_INFO(strcat(TAG,__VA_ARGS__));  \
  }
#else
  #define RTT_LOG_INFO(...) {}
#endif // RTT_LOG_EN

typedef enum {
    ADS_TASK_CMD_INIT,      // начальная инициализация АЦП
    ADS_TASK_CMD_DATA,      // приняты данные
    ADS_TASK_CMD_START,     // команда на запуск измерений
    ADS_TASK_CMD_STOP,      // команда на остановку измерений
    ADS_TASK_CMD_SINGLE,    // команда на запуск одиночного измерения
    ADS_TASK_CMD_TERMINATE, // завершение работы задачи
} ads_task_cmd_e;






static QueueHandle_t m_q_cmd = NULL; // очередь команд управления
static ads129x_handle_t m_adc0_handle = NULL; // хендл для работы с АЦП0
static ads129x_handle_t m_adc1_handle = NULL; // хендл для работы с АЦП1
static TaskHandle_t m_ads_task = NULL; // управляющая задача
static uint8_t m_spiDevID = 0xFF; // ID интерефейса SPI
static ads_task_callback_t m_callback = NULL; // функция верхнего уровня, в которую передаются принятые данные





static void ads_rdy_isr(void)
{ // обработчик перывания от RDY
  ads_task_cmd_e cmd = ADS_TASK_CMD_DATA;
  xQueueSendFromISR(m_q_cmd, &cmd, NULL);
}

// отправка команды в ads_task
static bool ads_send_cmd(ads_task_cmd_e cmd)
{
    if(m_q_cmd == NULL) return false;
    return xQueueSend(m_q_cmd, &cmd, 0);
}


// в этом потоке осуществляется прием и обработка данных с двух АЦП
static void ads_task(void *args)
{
    ads_task_cmd_e cmd;
    adstask_data_t ads_data;
    ads1299_config_t adc0_cfg;
    ads1299_config_t adc1_cfg;
    bool single_shot = false;

    ads_send_cmd(ADS_TASK_CMD_INIT);

    for(;;) {
        if(pdTRUE != xQueueReceive(m_q_cmd, &cmd, portMAX_DELAY)) {
            RTT_LOG_INFO("Ошибка ожидания команды управления");
            break;
        }

        switch (cmd) {
            case ADS_TASK_CMD_DATA: // требуется прочитать очередную порцию данных
            {
                RTT_LOG_INFO("ADS_TASK_CMD_DATA");
                memset(&ads_data, 0, sizeof(ads_data));
                ads129x_data_t data;
                // читаю данных из АЦП 0
                uint16_t err = ads1299_get_data(m_adc0_handle, &data);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("Ошибка чтения данных из АЦП 0");
                    break;
                }

                // сохраняю прочитанные данные в буфер
                ads_data.adc0_status = data.status;
                memcpy(&ads_data.adc0, &data.ch[0], sizeof(ads_data.adc0));

                // читаю данные из АЦП 1
                err = ads1299_get_data(m_adc1_handle, &data);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("Ошибка чтения данных из АЦП 1");
                    break;
                }

                // сохраняю прочитанные данные в буфер
                ads_data.adc1_status = data.status;
                memcpy(&ads_data.adc1, &data.ch[0], sizeof(ads_data.adc1));

                // вызываю колбэк и передаю данные на верхний уровень
                if(m_callback) {
                    m_callback(&ads_data);
                }
            }
            break;

            case ADS_TASK_CMD_START:
                RTT_LOG_INFO("ADS_TASK_CMD_START");
                // TODO разрешаю прерывания от АЦП
                ADS129X_START(); // запускаю измерения
                if(single_shot) {
                    ads_send_cmd(ADS_TASK_CMD_STOP);
                    single_shot = false;
                }
            break;

            case ADS_TASK_CMD_STOP:
                RTT_LOG_INFO("ADS_TASK_CMD_STOP");
                // запрещаю прерывания от АЦП
                ADS129X_INT_DISABLE();
                ADS129X_STOP(); // останавливаю измерения
            break;

            case ADS_TASK_CMD_SINGLE:
            {
                RTT_LOG_INFO("ADS_TASK_CMD_START");
                // установить бит "single_shot" и запустить измерения
                ads1299_config4_t cfg4 = adc0_cfg.config4;
                cfg4.single_shot = 1;
                uint16_t err = ads1299_set_reg(m_adc0_handle, ADS1299_REG_CONFIG4, *(uint8_t *)&cfg4);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("Ошибка записи в регистр config4");
                    break;
                }
                cfg4 = adc1_cfg.config4;
                cfg4.single_shot = 1;
                err = ads1299_set_reg(m_adc1_handle, ADS1299_REG_CONFIG4, *(uint8_t *)&cfg4);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("Ошибка записи в регистр config4");
                    break;
                }
                single_shot = true;
                ads_send_cmd(ADS_TASK_CMD_START);
            }
            break;

            case ADS_TASK_CMD_TERMINATE:
                RTT_LOG_INFO("ADS_TASK_CMD_TERMINATE");
            break;

            case ADS_TASK_CMD_INIT:
            {
                RTT_LOG_INFO("ADS_TASK_CMD_INIT");
                single_shot = false;
                // начальное конфигурирование АЦП
                // создаю дефолтный конфиг
                ads1299_def_config(&adc0_cfg); 
                ads1299_def_config(&adc1_cfg);
                // изменяю конфигурацию
                // TODO
                // заливаю новые конфиги в АЦП
                uint16_t err = ads1299_init(m_adc0_handle, &adc0_cfg);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("Ошибка при инициализации АЦП 0")
                }
                err = ads1299_init(m_adc1_handle, &adc1_cfg);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("Ошибка при инициализации АЦП 1")
                    break;
                }
                RTT_LOG_INFO("Инициализация АЦП0 и АЦП1 успешно завершена");
            }
            break;
            
            default:    
            break;
        }

        if(cmd == ADS_TASK_CMD_TERMINATE) break;
    } // for

    if(m_q_cmd) {
        vQueueDelete(m_q_cmd);
        m_q_cmd = NULL;
    }
    ads_task_deinit();
}






// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Начальная инициализация модуля
 * 
 * callback - Адрес функции обратного вызова
 * 
 * return 
 *  ERR_NOERROR - если ошибок нет
 *  ERR_OUT_OF_MEMORY - недостаточно памяти
*/
uint16_t ads_task_init(ads_task_callback_t callback)
{
    uint16_t err;

    do{
        err = spiInit(&m_spiDevID, SPIM3, SPIM3_IRQn, SPIM3_PRIORITY, SPIM3_FREQUENCY);
        if(err != ERR_NOERROR)
        {
          RTT_LOG_INFO("MAIN: Can't init SPIM3!");
          break;
        }
        
        ADS129X_PWDN_OFF(); // режим пониженного потребления выключен
        ADS129X_RESET_OFF(); // сброс выключен

        ANA_PWR_ON(); // подаю напряжение на аналоговую часть
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // требование по даташиту после подачи питания

        // инициализация устройств на шине
        err = ads129x_add(m_spiDevID, ADS129X_CS0_PIN, &m_adc0_handle);
        if(err != ERR_NOERROR) {
            break;
        }
        err = ads129x_add(m_spiDevID, ADS129X_CS1_PIN, &m_adc1_handle);
        if(err != ERR_NOERROR) {
            break;
        }

        // настраиваю прерывание от АЦП
        ADS129X_RDY_INIT();
        sysSetGpioteHook(GPIOTE_CH_ADS129X, ads_rdy_isr);
        ADS129X_INT_ENABLE();
        
        // создаю задачу для реализации логики работы
        if(pdTRUE != xTaskCreate(ads_task, "ADS TASK", ADSTASK_STACK_SIZE, NULL, ADSTASK_PRIORITY, &m_ads_task)) {
            err = ERR_OUT_OF_MEMORY;
            break;
        }

        // создаю очередь для управляющих команд
        m_q_cmd = xQueueCreate(ADSTASK_CMD_QUEUE_SIZE, sizeof(ads_task_cmd_e));
        if(m_q_cmd == NULL) {
            err = ERR_OUT_OF_MEMORY;
            break;
        }

        m_callback = callback;

        // конфигурирование АЦП происходит уже в управляющей задаче
    }while(0);

    // в случае ошибки освобождаю ресурсы
    if(err != ERR_NOERROR) {
        if(m_q_cmd) {
            vQueueDelete(m_q_cmd);
            m_q_cmd = NULL;
        }
        ads_task_deinit();
    }

    return err;
}


/**
 * Деинициализация модуля
 * 
*/
uint16_t ads_task_deinit(void)
{
    // отправляю управляющую команду в задачу
    if(m_q_cmd) {
        if(ads_send_cmd(ADS_TASK_CMD_TERMINATE)) return ERR_NOERROR;
    }
    
    // TODO запрещаю прерывания от АЦП
    // удаляю используемые ресурсы
    ANA_PWR_OFF();
    if(m_adc0_handle) {
        ads129x_remove(m_adc0_handle);
        m_adc0_handle = NULL;
    }
    if(m_adc1_handle) {
        ads129x_remove(m_adc1_handle);
        m_adc1_handle = NULL;
    }
    if(m_spiDevID != 0xFF) {
        spiDeInit(m_spiDevID);
        m_spiDevID = 0xFF;
    }
    if(m_ads_task) {
        vTaskDelete(m_ads_task);
        m_ads_task = NULL;
    }
    m_callback = NULL;
    
    return ERR_NOERROR;
}


/**
 * Запуск измерений
 * 
 * single_shot - флаг запуска одиночного измерения
 * 
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
*/
uint16_t ads_task_start(bool single_shot)
{
    // проверяю, была ли начальная инициализация
    if(m_ads_task == NULL) return ERR_NOT_INITED;
    // отправляю команду на запуск измерений
    if(single_shot) {
        if(!ads_send_cmd(ADS_TASK_CMD_STOP)) return ERR_FIFO_OVF;
        if(!ads_send_cmd(ADS_TASK_CMD_SINGLE)) return ERR_FIFO_OVF;
    }else{
        if(!ads_send_cmd(ADS_TASK_CMD_START)) return ERR_FIFO_OVF;
    }
    return ERR_NOERROR;
}


/**
 * Останов измерений
 * 
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
*/
uint16_t ads_task_stop(void)
{
    // проверяю, была ли начальная инициализация
    if(m_ads_task == NULL) return ERR_NOT_INITED;
    // отправляю команду на остановку измерений
    if(!ads_send_cmd(ADS_TASK_CMD_STOP)) return ERR_FIFO_OVF;
    return ERR_NOERROR;
}


