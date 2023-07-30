/**
 * Модуль реализует логику работы с двумя АЦП
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
#include "ads1298.h"
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
    NRF_LOG_INFO(__VA_ARGS__);  \
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
    ADS_TASK_CMD_GET_CFG,   // запрос конфига
    ADS_TASK_CMD_SET_CFG,   // установка нового конфига
} ads_task_cmd_e;

// структрура для команд чтения и записи кофигурации из вне
typedef struct {
  adstask_adc_no_e  adc_no; // номер АЦП
  uint8_t           *buff;  // указатель на буфер
} ads_cmd_cfg_t;

// формат очереди заданий
typedef struct {
  ads_task_cmd_e    cmd;    // код задания
  void            * args;  // дополнительные входные данные (может и не быть)
} ads_task_cmd_t;






static QueueHandle_t m_q_cmd = NULL; // очередь команд управления
static ads129x_handle_t m_adc0_handle = NULL; // хендл для работы с АЦП0
static ads129x_handle_t m_adc1_handle = NULL; // хендл для работы с АЦП1
static TaskHandle_t m_ads_task = NULL; // управляющая задача
static uint8_t m_spiDevID = 0xFF; // ID интерефейса SPI
static ads_task_callback_t m_callback = NULL; // функция верхнего уровня, в которую передаются принятые данные
static QueueHandle_t m_q_res = NULL; // очередь для передачи результата выполнения команды (используется в некоторых командах)
static bool m_is_started = false;
static SemaphoreHandle_t m_mutex = NULL; // мьютекс для ограничения множественных вызовов некоторых функций




static void ads_rdy_isr(void)
{ // обработчик перывания от RDY
  ads_task_cmd_t cmd = {
    .cmd = ADS_TASK_CMD_DATA
  };
  xQueueSendFromISR(m_q_cmd, &cmd, NULL);
}

// отправка команды в ads_task без дополнительных данных
static bool ads_send_cmd(ads_task_cmd_e cmd)
{
    if(m_q_cmd == NULL) return false;
  
    ads_task_cmd_t command = {
      .cmd = cmd
    };
  
    return xQueueSend(m_q_cmd, &command, 0);
}

// отправка команды в ads_task с дополнительными данными
static bool ads_send_cmd_args(ads_task_cmd_e cmd, void *args)
{
    if(m_q_cmd == NULL) return false;
  
    ads_task_cmd_t command = {
      .cmd = cmd,
      .args = args
    };
  
    return xQueueSend(m_q_cmd, &command, 0);
}

static int32_t sample24bitToInt32(ads129x_24bit_t sample)
{ // преобразует сэмпл одного канала АЦП 24 бит в int32_t
  int32_t res = 0;
  if(sample.val[0] & 0x80) res = 0xFF000000;
  res += ((int32_t)sample.val[0] << 16) + ((int32_t)sample.val[1] << 8) + sample.val[2];
  return res;
}

static uint32_t sample24bitToUint32(ads129x_24bit_t sample)
{ // преобразует сэмпл одного канала АЦП 24 бит в int32_t
  uint32_t res = ((int32_t)sample.val[0] << 16) + ((int32_t)sample.val[1] << 8) + sample.val[2];
  return res;
}

static void setupChannel(ads1298_chset_t *ch, uint8_t mux, uint8_t gain, uint8_t pd)
{ // настройка канала АЦП
   ch->gain = gain;
   ch->mux = mux;
   ch->pd = pd;
}


// в этом потоке осуществляется прием и обработка данных с двух АЦП
static void ads_task(void *args)
{
    ads_task_cmd_t cmd;
    adstask_data_t ads_data;
    ads1298_config_t adc0_cfg;
    ads1298_config_t adc1_cfg;
    bool single_shot = false;
  
    uint32_t sample_cnt = 0; // TEST
  
    // TEST
//    ads129x_data_t data_const;
//    memset(&data_const, 0, sizeof(data_const));
//    for(uint8_t i=0; i < 8; i++)
//    {
//      data_const.ch[i].val[0] = 0xFC;
//      data_const.ch[i].val[1] = 0x18;
//      data_const.ch[i].val[2] = 0xAA;
//    }

    ads_send_cmd(ADS_TASK_CMD_INIT);

    for(;;) {
        if(pdTRUE != xQueueReceive(m_q_cmd, &cmd, portMAX_DELAY)) {
            RTT_LOG_INFO("ADSTASK: Cmd timeout");
            break;
        }

        switch (cmd.cmd) {
            case ADS_TASK_CMD_DATA: // требуется прочитать очередную порцию данных
            {
                memset(&ads_data, 0, sizeof(ads_data));
                ads129x_data_t data;

                // читаю данных из АЦП 0
                uint16_t err = ads1298_get_data(m_adc0_handle, &data);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("ADSTASK: Read ADC 0 error 0x%04X", err);
                    break;
                }
                
//                if(sample_cnt == 3)
//                {
//                  NRF_LOG_INFO("adc_data0:");
//                  NRF_LOG_HEXDUMP_INFO(&data, sizeof(data)); 
//                }

                // сохраняю прочитанные данные в буфер
                ads_data.adc0_status = sample24bitToUint32(data.status);
                for(uint8_t i = 0; i < ADS129X_CH_CNT; i++)
                {
                  ads_data.adc0[i] = sample24bitToInt32(data.ch[i]);
                }
//                ads_data.adc0_status = sample24bitToUint32(data.status);
//                for(uint8_t i = 0; i < ADS129X_CH_CNT; i++)
//                {
//                  ads_data.adc0[i] = sample24bitToInt32(data_const.ch[i]);
//                }

                // читаю данные из АЦП 1
                err = ads1298_get_data(m_adc1_handle, &data);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("ADSTASK: Read ADC 1 error 0x%04X", err);
                    break;
                }
                
//                if(sample_cnt == 3)
//                {
//                  NRF_LOG_INFO("adc_data1:");
//                  NRF_LOG_HEXDUMP_INFO(&data, sizeof(data)); 
//                }

                // сохраняю прочитанные данные в буфер
                ads_data.adc1_status = sample24bitToUint32(data.status);
                for(uint8_t i = 0; i < ADS129X_CH_CNT; i++)
                {
                  ads_data.adc1[i] = sample24bitToInt32(data.ch[i]);
                }
//                ads_data.adc1_status = sample24bitToUint32(data.status);
//                for(uint8_t i = 0; i < ADS129X_CH_CNT; i++)
//                {
//                  ads_data.adc1[i] = sample24bitToInt32(data_const.ch[i]);
//                }

                
                // === сюда можно вставить какую-либо обработку данных ===
                
                

                // вызываю колбэк и передаю данные на верхний уровень
                if(m_callback) {
                    m_callback(&ads_data);
                }
                
                sample_cnt++;
            }
            break;

            case ADS_TASK_CMD_START:
                RTT_LOG_INFO("ADS_TASK_CMD_START");
                sample_cnt = 0;
                // разрешаю прерывания от АЦП
                ADS129X_INT_ENABLE();
                ADS129X_START(); // запускаю измерения
                //ads129x_cmd(m_adc0_handle, ADS129X_CMD_START, pdMS_TO_TICKS(100)); // TEST
                //ads129x_cmd(m_adc1_handle, ADS129X_CMD_START, pdMS_TO_TICKS(100)); // TEST
            
                if(single_shot) {
                    // TODO установить глобальный флаг, по которому выполнить команду ADS_TASK_CMD_STOP после получения данных
                    ads_send_cmd(ADS_TASK_CMD_STOP);
                    single_shot = false;
                }
                m_is_started = true;
            break;

            case ADS_TASK_CMD_STOP:
                RTT_LOG_INFO("ADS_TASK_CMD_STOP");
                RTT_LOG_INFO("ADSTASK: sample_cnt = %d", --sample_cnt);
                // запрещаю прерывания от АЦП
                ADS129X_INT_DISABLE();
                ADS129X_STOP(); // останавливаю измерения
                m_is_started = false;
            break;

            case ADS_TASK_CMD_SINGLE:
            {
                RTT_LOG_INFO("ADS_TASK_CMD_SINGLE");
                // установить бит "single_shot" и запустить измерения
                ads1298_config4_t cfg4 = adc0_cfg.config4;
                cfg4.single_shot = 1;
                uint16_t err = ads1298_set_reg(m_adc0_handle, ADS1298_REG_CONFIG4, *(uint8_t *)&cfg4);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("ADSTASK: Write to config4 ADC 0 error 0x%04X", err);
                    break;
                }
                cfg4 = adc1_cfg.config4;
                cfg4.single_shot = 1;
                err = ads1298_set_reg(m_adc1_handle, ADS1298_REG_CONFIG4, *(uint8_t *)&cfg4);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("ADSTASK: Write to config4 ADC 1 error 0x%04X", err);
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
                ads1298_def_config(&adc0_cfg); 
                ads1298_def_config(&adc1_cfg);
              
                // ИЗМЕНЕНИЕ ДЕФОЛТНОЙ КОНФИГУРАЦИИ
                uint8_t mux = ADS1298_MUX_NORMAL;
                uint8_t gain = ADS1298_GAIN_12;
                setupChannel(&adc0_cfg.ch1set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch2set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch3set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch4set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch5set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch6set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch7set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc0_cfg.ch8set, mux, gain, ADS1298_PD_NORMAL);
              
                setupChannel(&adc1_cfg.ch1set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch2set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch3set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch4set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch5set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch6set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch7set, mux, gain, ADS1298_PD_NORMAL);
                setupChannel(&adc1_cfg.ch8set, mux, gain, ADS1298_PD_NORMAL);
                
                // настройка точки Вилсона
                adc0_cfg.wct1.wcta = ADS1298_WCTA_CH1P;
                adc0_cfg.wct1.pd_wcta = 1;
                adc0_cfg.wct2.wctb = ADS1298_WCTB_CH1N;
                adc0_cfg.wct2.wctc = ADS1298_WCTC_CH2P;
                adc0_cfg.wct2.pd_wctb = 1;
                adc0_cfg.wct2.pd_wctc = 1;

                // заливаю новые конфиги в АЦП
                uint16_t err = ads1298_init(m_adc0_handle, &adc0_cfg);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("ADSTASK: Init ADC 0 error 0x%04X", err)
                }
                err = ads1298_init(m_adc1_handle, &adc1_cfg);
                if(err != ERR_NOERROR) {
                    RTT_LOG_INFO("ADSTASK: Init ADC 1 error 0x%04X", err)
                    break;
                }
                RTT_LOG_INFO("ADSTASK: Init ADC 0 and ADC 1 COMPLETE");
            }
            break;
            
            case ADS_TASK_CMD_GET_CFG:   // запрос конфига
            {
              ads_cmd_cfg_t *cmd_args = (ads_cmd_cfg_t *)cmd.args;
              uint16_t err = ERR_UNKNOWN;
              do{
                if(cmd_args == NULL) break;
                
                ads129x_handle_t handle = NULL;
                switch(cmd_args->adc_no)
                {
                  case ADSTASK_ADC_MASTER:
                    handle = m_adc0_handle;
                  break;
                  
                  case ADSTASK_ADC_SLAVE:
                    handle = m_adc1_handle;
                  break;
                  
                  default:
                    err = ERR_INVALID_PARAMETR;
                    break;
                }
                if(handle == NULL) break;
                
                err = ads1298_get_regs(handle, ADS1298_REG_ID, ADS1298_REG_LAST + 1, (uint8_t *)cmd_args->buff);
                if(err != ERR_NOERROR)
                {
                  RTT_LOG_INFO("ADSTASK: Read regs error 0x%04X", err);
                  break;
                }
              }while(0);
              
              xQueueSend(m_q_res, &err, 0);
            }
            break;
            
            case ADS_TASK_CMD_SET_CFG:   // установка нового конфига
              
            break;
            
            default:    
            break;
        }

        if(cmd.cmd == ADS_TASK_CMD_TERMINATE) break;
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
          RTT_LOG_INFO("ADSTASK: Can't init SPIM3!");
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
        
        // создаю задачу для реализации логики работы
        if(pdTRUE != xTaskCreate(ads_task, "ADS TASK", ADSTASK_STACK_SIZE, NULL, ADSTASK_PRIORITY, &m_ads_task)) {
            err = ERR_OUT_OF_MEMORY;
            break;
        }

        // создаю очередь для управляющих команд
        m_q_cmd = xQueueCreate(ADSTASK_CMD_QUEUE_SIZE, sizeof(ads_task_cmd_t));
        if(m_q_cmd == NULL) {
            err = ERR_OUT_OF_MEMORY;
            break;
        }
        
        m_q_res = xQueueCreate(1, sizeof(uint16_t));
        if(m_q_res == NULL) {
            err = ERR_OUT_OF_MEMORY;
            break;
        }
        
        m_mutex = xSemaphoreCreateMutex();
        if(m_mutex == NULL) {
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
    if(m_q_res) {
        vQueueDelete(m_q_res);
        m_q_res = NULL;
    }
    if(m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = NULL;
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


/**
 * Запрос конфига
 * 
 * adc_no - номер АЦП
 * cfg_srt - строка конфига в формате: n,rrvv,....,rrvv где n - номер АЦП (0 или 1), rrvv - uint16_t, где rr - адрес регистра, vv - значение регистра
 * cfg_len_max - максимальный размер буфер под строку с конфигом
 * timeout_ms - максимальное время ожидания начала выполенния задания
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
 *  ERR_INVALID_PARAMETR - ошибка входных данных
 *  ERR_TIMEOUT - таймаут ожидания доступа
 *  ERR_INVALID_STATE - АЦП в процессе измерения
*/
uint16_t ads_task_get_config(adstask_adc_no_e adc_no, char *cfg_str, uint8_t cfg_len_max, uint32_t timeout_ms)
{
  // проверяю, была ли начальная инициализация
  if(m_ads_task == NULL) return ERR_NOT_INITED;
  if((cfg_str == NULL) || (cfg_len_max < 7)) return ERR_INVALID_PARAMETR;
  if(m_is_started) return ERR_INVALID_STATE; // идет процесс измерений
  if((uint8_t)adc_no >= ADS129X_CNT) return ERR_INVALID_PARAMETR;
  
  if(pdTRUE != xSemaphoreTake(m_mutex, pdMS_TO_TICKS(timeout_ms))) return ERR_TIMEOUT;
  
  uint8_t buff[ADS1298_REG_LAST + 1]; // буфер под конфиг
  char regval[6]; // буфер для очередного значения
  uint16_t err = ERR_NOERROR;
  
  do{
    
    ads_cmd_cfg_t args = {
      .adc_no = adc_no,
      .buff = buff
    };
    
    // отправляю задание на получение конфига
    if(!ads_send_cmd_args(ADS_TASK_CMD_GET_CFG, &args)) 
    {
      err = ERR_FIFO_OVF;
      break; // на выход, если очередь переоплнена
    }
    
    if(pdTRUE != xQueueReceive(m_q_res, &err, pdMS_TO_TICKS(timeout_ms)))
    {
      err = ERR_TIMEOUT;
      break;
    }
    
    if(err != ERR_NOERROR) break; // чтение регистров не было выполнено
    
    snprintf(cfg_str, cfg_len_max, "%d", (uint8_t)adc_no);
    // конфигурация прочитана в буфер, преобразую ее в заданный вид
    for(uint8_t i = 0; i <= ADS1298_REG_LAST; i++)
    {
      snprintf(regval, sizeof(regval), ",%02X%02X", i, buff[i]);
      if((strlen(cfg_str) + strlen(regval)) >= cfg_len_max) break;
      strcat(cfg_str, regval);
    }
  }while(0);
  
  xSemaphoreGive(m_mutex);
  return err;
}

/**
 * Сохранение регистра
 * 
 * adc_no - номер АЦП
 * reg_addr - адрес регистра
 * reg_val - значение регистра
 * timeout_ms - максимальное время ожидания начала выполенния задания
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED - модуль не инициализирован
 *  ERR_FIFO_OVF - переполнение очереди команд
 *  ERR_INVALID_PARAMETR - ошибка входных данных
 *  ERR_TIMEOUT - таймаут ожидания доступа
 *  ERR_INVALID_STATE - АЦП в процессе измерения
*/
uint16_t ads_task_set_reg(adstask_adc_no_e adc_no, uint8_t reg_addr, uint8_t reg_val, uint32_t timeout_ms)
{
  // проверяю, была ли начальная инициализация
  if(m_ads_task == NULL) return ERR_NOT_INITED;
  if(m_is_started) return ERR_INVALID_STATE; // идет процесс измерений
  if((uint8_t)adc_no >= ADS129X_CNT) return ERR_INVALID_PARAMETR;
  if(reg_addr > ADS1298_REG_LAST) return ERR_INVALID_PARAMETR;
  
  if(pdTRUE != xSemaphoreTake(m_mutex, pdMS_TO_TICKS(timeout_ms))) return ERR_TIMEOUT;
  
  uint16_t err = ERR_NOERROR;
  
  do{
    ads129x_handle_t handle = NULL;
    switch(adc_no)
    {
      case ADSTASK_ADC_MASTER:
        handle = m_adc0_handle;
      break;
                  
      case ADSTASK_ADC_SLAVE:
        handle = m_adc1_handle;
      break;
                  
      default:
        err = ERR_INVALID_PARAMETR;
        break;
      }
    if(handle == NULL) break;
    err = ads1298_set_reg(handle, reg_addr, reg_val);
  }while(0);
  
  xSemaphoreGive(m_mutex);
  return err;  
}

