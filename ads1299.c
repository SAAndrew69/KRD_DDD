/**
 * Реализация функций для работы с ADS1299
 * 
 * 11.07.2023 - Реализован базовый функционал модуля
 * 
 * ОСОБЕННОСТИ
 * - все функции блокируют выполенние потока до их завершения
*/


#include "ads1299.h"
#include "nrf.h"



#define ADS1299_ACCESS_TO_SPI_TIMEOUT_MS    200     // таймаут ожидания доступа к шине SPI





/**
 * Дефолтный конфиг
 * 
 * config - Указатель на буфер для размещения конфигурации
 * return
 * Возвращает дефолтную конфигурацию
*/
void ads1299_def_config(ads1299_config_t *config)
{
    ASSERT(config != NULL);

    ads1299_config1_t config1 = {
        .dr = ADS1299_DR_500SPS,    // битрейт 500 отсчетов в секунду
        .bit4 = 1,
        .clk_en = 0,                // клок на выход не передается
        .daisy_en = 1,              // Multiple readback mode
        .bit7 = 1
    };
    ads1299_config2_t config2 = {
        .cal_freq = ADS1299_CAL_FREQ_3, // тестовый сигнал - DC
        .cal_amp = 0,               // амплитуда: 1 × –(VREFP – VREFN) / 2400
        .int_cal = 1,               // тестовый сигнал генерится внутри
        .bit6 = 1,
        .bit7 = 1
    };
    ads1299_config3_t config3 = {
        .bias_stat = 0,             // BIAS отключен
        .bias_loff_sens = 0,        // BIAS sense is disabled
        .pd_bias = 0,               // BIAS buffer is powered down
        .biasref_int = 1,           // BIASREF signal (AVDD + AVSS) / 2 generated internally
        .bias_meas = 0,             // Измерения не проводятся
        .bit5 = 1,
        .bit6 = 1,
        .pd_refbuf = 0              // Power-down internal reference buffer
    };
    ads1299_config4_t config4 = {
        .pd_loff_comp = 0,          // Lead-off comparators disabled
        .single_shot = 0            // Continuous conversion mode
    };
    ads1299_loff_t  loff = {
        .flead_off = ADS1298_FLEAD_OFF_DC,  // DC lead-off detection
        .ilead_off = ADS1299_ILEAD_OFF_6N,  // 6nA
        .comp_th = ADS1299_COMP_THP_95      // 95%
    };
    ads1299_chset_t chset = {
        .mux = ADS1298_MUX_TEST,    // Test signal
        .srb2 = 0,                  // Open
        .gain = ADS1299_GAIN_1,     // Усиление 1
        .pd = 0,                    // Канал подключен
    };
    ads1299_gpio_t  gpio = {
        .ctrl1 = 1,                 // вход
        .crtl2 = 1,                 // вход
        .ctrl3 = 1,                 // вход
        .ctrl4 = 1                  // вход
    };
    ads1299_misc1_t misc1 = {
        .srb1 = 0                   // Switches open
    }

    memset(config, 0, sizeof(ads1299_config_t));

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
    config->beas_sensp = 0;
    config->beas_sensn = 0;
    config->loff_sensp = 0;
    config->loff_sensn = 0;
    config->loff_flip = 0;
    config->gpio = gpio;
    config->misc1 = misc1;
    config->misc2 = 0;
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
uint16_t ads1299_init(ads129x_handle_t handle, ads1299_config_t *config)
{
    ASSERT(handle != NULL);

    // перевожу в командный режим
    ERROR_CHECK(ads129x_cmd(handle, ADS129X_CMD_SDATAC, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS));

    // делаю программный сброс
    ERROR_CHECK(ads129x_cmd(handle, ADS129X_CMD_RESET, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS));
    nrf_delay_us(10);

    if(config == NULL) return ERR_NOERROR;

    // заливаю конфигурацию (если задана)
    ERROR_CHECK(ads129x_write_regs(handle, ADS1299_REG_CONFIG1, 17, (uint8_t *)&config->config1, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS));
    return ads129x_write_regs(handle, ADS1299_REG_GPIO, 4, (uint8_t *)&config->gpio, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
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
uint16_t ads1299_get_config(ads129x_handle_t handle, ads1299_config_t *config)
{
    ASSERT((handle != NULL) || (config != NULL));

    ERROR_CHECK(ads129x_read_regs(handle, ADS1299_REG_CONFIG1, 17, (uint8_t *)&config->config1, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS));
    return ads129x_read_regs(handle, ADS1299_REG_GPIO, 4, (uint8_t *)&config-gpio, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
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
uint16_t ads1299_set_config(ads129x_handle_t handle, ads1299_config_t *config)
{
    ASSERT((handle != NULL) || (config != NULL));

    ERROR_CHECK(ads129x_write_regs(handle, ADS1299_REG_CONFIG1, 17, (uint8_t *)&config->config1, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS));
    return ads129x_write_regs(handle, ADS1299_REG_GPIO, 4, (uint8_t *)&config->gpio, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
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
uint16_t ads1299_set_config(ads129x_handle_t handle, uint8_t ch_no, ads1299_chset_t ch_config)
{
    ASSERT(handle != NULL);

    // проверка валидности входных данных
    if(ch_no > ADS1299_CH_8) return ERR_INVALID_PARAMETR;

    uint8_t addr = ADS1299_CH_1 + ch_no;

    return ads129x_write_reg(handle, addr, *(uint8_t *)&ch_config, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
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
uint16_t ads1299_set_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t val)
{
    ASSERT(handle != NULL);

    if(reg_addr > ADS1299_REG_CONFIG4) return ERR_INVALID_PARAMETR;

    return ads129x_write_reg(handle, reg_addr, val, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
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
uint16_t ads1299_get_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t *val)
{
    ASSERT((handle != NULL) || (val != NULL));

    if(reg_addr > ADS1299_REG_CONFIG4) return ERR_INVALID_PARAMETR;

    return ads129x_read_regs(handle, reg_addr, 1, val, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
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
uint16_t ads1299_get_data(ads129x_handle_t handle, ads129x_data_t *data)
{
    ASSERT((handle != NULL) || (data != NULL));

    return ads129x_read_data(handle, data, ADS1299_ACCESS_TO_SPI_TIMEOUT_MS);
}