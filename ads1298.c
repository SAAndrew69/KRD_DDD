/**
 * Реализация функций для работы с ADS1298
 * 
 * ОСОБЕННОСТИ
 * - все функции блокируют выполенние потока до их завершения
*/


#include "ads1298.h"
#include "nrf.h"
#include "stdlib.h"
#include "string.h"
#include "errors.h"
#include "nrf_delay.h"

#include "FreeRTOS.h"


#define ADS1298_ACCESS_TO_SPI_TIMEOUT_MS    200     // таймаут ожидания доступа к шине SPI



/**
 * Дефолтный конфиг
 * 
 * config - Указатель на буфер для размещения конфигурации
 * return
 * Возвращает дефолтную конфигурацию
*/
void ads1298_def_config(ads1298_config_t *config)
{
    ASSERT(config != NULL);

    ads1298_config1_t config1 = {
        .dr = ADS1298_DR_250SPS,    // битрейт 500 отсчетов в секунду при hr = 1
        .bit4 = 0,
        .clk_en = 0,                // клок на выход не передается
        .daisy_en = 1,              // Multiple readback mode
        .hr = 1                     // High-resolution
    };
    ads1298_config2_t config2 = {
        .test_freq = ADS1298_TEST_FREQ_3, // тестовый сигнал - DC
        .test_amp = 0,               // амплитуда: 1 × –(VREFP – VREFN) / 2400
        .int_test = 1,               // тестовый сигнал генерится внутри
        .wct_chop = 0                // Chopping frequency varies
    };
    ads1298_config3_t config3 = {
        .rld_stat = 0,              // RLD отключен
        .rld_loff_sens = 0,         // RLD sense is disabled
        .pd_rld = 0,                // RLD buffer is powered down
        .rldref_int = 1,            // RLDREF signal (AVDD + AVSS) / 2 generated internally
        .rld_meas = 0,              // Измерения не проводятся
        .vref_4v = 0,               // VREFP is set to 2.4 V
        .bit6 = 1,
        .pd_refbuf = ADS1298_PD_REF_ON // Use internal reference buffer
    };
    ads1298_config4_t config4 = {
        .resp_freq = ADS1298_RESP_FREQ_64K, // 64 kHz modulation clock
        .pd_loff_comp = 0,          // Lead-off comparators disabled
        .single_shot = 0,           // Continuous conversion mode
        .wct_rld = 0                // WCT to RLD connection off
    };
    ads1298_loff_t  loff = {
        .flead_off = ADS1298_FLEAD_OFF_LOFF,  // OFF
        .ilead_off = ADS1298_ILEAD_OFF_6N,  // 6nA
        .comp_th = ADS1298_COMP_THP_95,     // 95%
        .vlead_off_en = 0                   // Current source mode lead-off
    };
    ads1298_chset_t chset = {
        .mux = ADS1298_MUX_SHORT,   // Input shorted (for offset or noise measurements)
        .gain = ADS1298_GAIN_12,    // Усиление 12
        .pd = ADS1298_PD_POWERDOWN, // Канал отключен
    };
    ads1298_gpio_t  gpio = {
        .ctrl_1 = 1,                 // вход
        .ctrl_2 = 1,                 // вход
        .ctrl_3 = 1,                 // вход
        .ctrl_4 = 1                  // вход
    };
    ads1298_pace_t pace = {
        .pacee = ADS1298_PACE_EVEN_CH2, // Channel 2
        .paceo = ADS1298_PACE_ODD_CH1,  // Channel 1
        .pd_pace = 0                    // Pace detect buffer turned off
    };
    ads1298_resp_t resp = {
        .resp_ctrl = ADS1298_RESP_CTRL_OFF, // No respiration
        .resp_ph = ADS1298_RESP_PH_22_5,    // 22.5 градуса
        .reserve = 1,
        .resp_mod_en1 = 0,                 // RESP modulation circuitry turned off
        .resp_demod_en1 = 0,               // RESP demodulation circuitry turned off
    };
    ads1298_wct1_t wct1 = {
        .wcta = ADS1298_WCTA_CH1P,
        .pd_wcta = 0,               // канал wcta выключен
        .avr_ch4 = 0,
        .avr_ch7 = 0,
        .avl_ch5 = 0,
        .avf_ch6 = 0
    };
    ads1298_wct2_t wct2 = {
        .wctc = ADS1298_WCTC_CH2P,
        .wctb = ADS1298_WCTB_CH1N,
        .pd_wctb = 0,               // канал wctb выключен
        .pd_wctc = 0                // канал wctc выключен
    };

    memset(config, 0, sizeof(ads1298_config_t));

    config->config1 = config1;
    config->config2 = config2;
    config->config3 = config3;
    config->config4 = config4;
    config->loff = loff;
    config->ch1set = chset;
    config->ch2set = chset;
    config->ch3set = chset;
    config->ch4set = chset;
    config->ch5set = chset;
    config->ch6set = chset;
    config->ch7set = chset;
    config->ch8set = chset;
    config->rld_sensp = 0;
    config->rld_sensn = 0;
    config->loff_sensp = 0;
    config->loff_sensn = 0;
    config->loff_flip = 0;
    config->gpio = gpio;
    config->pace = pace;
    config->resp = resp;
    config->wct1 = wct1;
    config->wct2 = wct2;
}


/**
 * Начальная инициализация микросхемы
 * 
 * handle - Хендл микросхемы на шине SPI
 * config - Указатель на конфигурацию
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
 * 
 * В процессе начальной инициализации происходит сброс и переход в командный режим работы
 * Если задан файл конфигурации, то происходит загрузка конфигурации
*/
uint16_t ads1298_init(ads129x_handle_t handle, ads1298_config_t *config)
{
    ASSERT(handle != NULL);

    // перевожу в командный режим
    ERROR_CHECK(ads129x_cmd(handle, ADS129X_CMD_SDATAC, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS));

    // делаю программный сброс
    ERROR_CHECK(ads129x_cmd(handle, ADS129X_CMD_RESET, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS));
    nrf_delay_us(10);
  
    // перевожу в командный режим
    ERROR_CHECK(ads129x_cmd(handle, ADS129X_CMD_SDATAC, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS));
  
    // читаю ID и проверяю ID микросхемы
    uint8_t partID;
    ERROR_CHECK(ads1298_get_reg(handle, ADS1298_REG_ID, (uint8_t *)&partID));
    if(partID != ADS1298_ID) return ERR_INVALID_PARAMETR;

    if(config == NULL) return ERR_NOERROR;

    // заливаю конфигурацию (если задана)
    return ads1298_set_config(handle, config);
}


/**
 * Запрос текущей конфигурации
 * 
 * handle - Хендл микросхемы на шине SPI
 * config - Указатель на буфер для размещения конфигурации
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_get_config(ads129x_handle_t handle, ads1298_config_t *config)
{
    ASSERT((handle != NULL) || (config != NULL));

    return ads129x_read_regs(handle, ADS1298_REG_CONFIG1, 25, (uint8_t *)&config->config1, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}


/**
 * @brief Установка новой конфигурации
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param config - Указатель на конфигурацию
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_set_config(ads129x_handle_t handle, ads1298_config_t *config)
{
    ASSERT((handle != NULL) || (config != NULL));

    ERROR_CHECK(ads129x_write_regs(handle, ADS1298_REG_CONFIG1, 17, (uint8_t *)&config->config1, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS));
    return ads129x_write_regs(handle, ADS1298_REG_GPIO, 6, (uint8_t *)&config->gpio, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}


/**
 * @brief Изменение настроек канала
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param ch_no - Номер канала
 * @param ch_config - Указатель на конфигурацию канала
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_set_chcfg(ads129x_handle_t handle, uint8_t ch_no, ads1298_chset_t ch_config)
{
    ASSERT(handle != NULL);

    // проверка валидности входных данных
    if(ch_no > ADS1298_CH_8) return ERR_INVALID_PARAMETR;

    uint8_t addr = ADS1298_CH_1 + ch_no;

    return ads129x_write_reg(handle, addr, *(uint8_t *)&ch_config, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}


/**
 * @brief Установка любого регистра в любое значение
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param reg_addr - Адрес регистра
 * @param val - Новое значение регистра
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_set_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t val)
{
    ASSERT(handle != NULL);

    if(reg_addr > ADS1298_REG_LAST) return ERR_INVALID_PARAMETR;

    return ads129x_write_reg(handle, reg_addr, val, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}


/**
 * @brief Чтение любого регистра
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param reg_addr - Адрес регистра
 * @param val - Прочитанное значение регистра
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_get_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t *val)
{
    ASSERT((handle != NULL) || (val != NULL));

    if(reg_addr > ADS1298_REG_LAST) return ERR_INVALID_PARAMETR;

    return ads129x_read_regs(handle, reg_addr, 1, val, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}


/**
 * Чтение нескольких регистров
 * 
 * handle - Хендл микросхемы на шине SPI
 * reg_addr - Адрес регистра
 * cnt - Число регистров для чтения
 * buff - Буфер для чтения
 * 
 * return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_get_regs(ads129x_handle_t handle, uint8_t reg_addr, uint8_t cnt, uint8_t *buff)
{
    ASSERT((handle != NULL) || (buff != NULL) || (cnt == 0));

    if((reg_addr + cnt) > (ADS1298_REG_LAST + 1)) return ERR_INVALID_PARAMETR;
  
    return ads129x_read_regs(handle, reg_addr, cnt, buff, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}


/**
 * @brief Чтение данных АЦП
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param data - Указатель на буфер размещения данных
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_get_data(ads129x_handle_t handle, ads129x_data_t *data)
{
    ASSERT((handle != NULL) || (data != NULL));

    return ads129x_read_data(handle, data, ADS1298_ACCESS_TO_SPI_TIMEOUT_MS);
}
