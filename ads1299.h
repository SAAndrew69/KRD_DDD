#ifndef ADS1299_H
#define ADS1299_H

/**
 * Описание регистров ADS1299
 * Описание функций для работы с ADS1299
 * 
*/


#include <stdbool.h>
#include <stdint.h>

#include "ads129x.h"


/// РЕГИСТРЫ ADS1299
#define ADS1299_REG_ID          0x00    ///< ID микросхемы
#define ADS1299_REG_CONFIG1     0x01    ///< Конфигурационный регистр 1
#define ADS1299_REG_CONFIG2     0x02    ///< Конфигурационный регистр 2
#define ADS1299_REG_CONFIG3     0x03    ///< Конфигурационный регистр 3
#define ADS1299_REG_LOFF        0x04    ///< Lead-Off Control Register
#define ADS1299_REG_CH1SET      0x05    ///< Индивидуальные настройки канала 1
#define ADS1299_REG_CH2SET      0x06    ///< Индивидуальные настройки канала 2
#define ADS1299_REG_CH3SET      0x07    ///< Индивидуальные настройки канала 3
#define ADS1299_REG_CH4SET      0x08    ///< Индивидуальные настройки канала 4
#define ADS1299_REG_CH5SET      0x09    ///< Индивидуальные настройки канала 5
#define ADS1299_REG_CH6SET      0x0A    ///< Индивидуальные настройки канала 6
#define ADS1299_REG_CH7SET      0x0B    ///< Индивидуальные настройки канала 7
#define ADS1299_REG_CH8SET      0x0C    ///< Индивидуальные настройки канала 8
#define ADS1299_REG_BIAS_SENSP  0x0D    ///< Bias Drive Positive Derivation Register
#define ADS1299_REG_BIAS_SENSN  0x0E    ///< Bias Drive Negative Derivation Register
#define ADS1299_REG_LOFF_SENSP  0x0F    ///< Positive Signal Lead-Off Detection Register
#define ADS1299_REG_LOFF_SENSN  0x10    ///< Negative Signal Lead-Off Detection Register
#define ADS1299_REG_LOFF_FLIP   0x11    ///< Lead-Off Flip Register
#define ADS1299_REG_LOFF_STATP  0x12    ///< Lead-Off Positive Signal Status Register
#define ADS1299_REG_LOFF_STATN  0x13    ///< Lead-Off Negative Signal Status Register
#define ADS1299_REG_GPIO        0x14    ///< General-Purpose I/O Register
#define ADS1299_REG_MISC1       0x15    ///< Miscellaneous 1 Register
#define ADS1299_REG_MISC2       0x16    ///< Miscellaneous 2 Register
#define ADS1299_REG_CONFIG4     0x17    ///< Конфигурационный регистр 4

/// БИТОВЫЕ МАСКИ, ЗНАЧЕНИЯ ПОЛЕЙ
#define ADS1299_ID_MASK         0x1F    ///< Маска для выделения ID микросхемы
#define ADS1299_ID              0x1E    ///< ID ADS1299 (8 каналов)

#define ADS1299_DR_16KSPS       0x00    ///< Output data rate Fclk/128 = 16 kSPS
#define ADS1299_DR_8KSPS        0x01    ///< 8 kSPS
#define ADS1299_DR_4KSPS        0x02    ///< 4 kSPS
#define ADS1299_DR_2KSPS        0x03    ///< 2 kSPS
#define ADS1299_DR_1KSPS        0x04    ///< 1 kSPS
#define ADS1299_DR_500SPS       0x05    ///< 500 SPS
#define ADS1299_DR_250SPS       0x06    ///< 250 SPS

#define ADS1299_CAL_FREQ_1      0x00    ///< Test signal frequency: fCLK / 2^21
#define ADS1299_CAL_FREQ_2      0x01    ///< Test signal frequency: fCLK / 2^22
#define ADS1299_CAL_FREQ_3      0x03    ///< Test signal frequency: DC

/// Lead-off comparator threshold (Comparator positive side)
#define ADS1299_COMP_THP_95     0x00    ///< 95%
#define ADS1299_COMP_THP_92     0x01    ///< 92.5%
#define ADS1299_COMP_THP_90     0x02    ///< 90%
#define ADS1299_COMP_THP_87     0x03    ///< 87.5%
#define ADS1299_COMP_THP_85     0x04    ///< 85%
#define ADS1299_COMP_THP_80     0x05    ///< 80%
#define ADS1299_COMP_THP_75     0x06    ///< 75%
#define ADS1299_COMP_THP_70     0x07    ///< 70%
/// Lead-off comparator threshold (Comparator negative side)
#define ADS1299_COMP_THN_5      0x00    ///< 5%
#define ADS1299_COMP_THN_7      0x01    ///< 7.5%
#define ADS1299_COMP_THN_10     0x02    ///< 10%
#define ADS1299_COMP_THN_12     0x03    ///< 12.5%
#define ADS1299_COMP_THN_15     0x04    ///< 15%
#define ADS1299_COMP_THN_20     0x05    ///< 20%
#define ADS1299_COMP_THN_25     0x06    ///< 25%
#define ADS1299_COMP_THN_30     0x07    ///< 30%

/// Lead-off current magnitude (determine the magnitude of current for the current lead-off mode)
#define ADS1299_ILEAD_OFF_6N    0x00    ///< 6nA
#define ADS1299_ILEAD_OFF_24N   0x01    ///< 24nA
#define ADS1299_ILEAD_OFF_6U    0x02    ///< 6uA
#define ADS1299_ILEAD_OFF_24U   0x03    ///< 24uA

/// Lead-off frequency (determine the frequency of lead-off detect for each channel)
#define ADS1299_FLEAD_OFF_DC    0x00    ///< DC lead-off detection
#define ADS1299_FLEAD_OFF_7_8HZ 0x01    ///< AC lead-off detection at 7.8 Hz (fCLK / 218)
#define ADS1299_FLEAD_OFF_31_2HZ 0x02   ///< AC lead-off detection at 31.2 Hz (fCLK / 216)
#define ADS1299_FLEAD_OFF_DR_4HZ 0x03   ///< AC lead-off detection at fDR / 4

/// Individual Channel Settings
#define ADS1299_MUX_NORMAL      0x00    ///< Normal electrode input
#define ADS1299_MUX_SHORT       0x01    ///< Input shorted (for offset or noise measurements)
#define ADS1299_MUX_CONJ        0x02    ///< Used in conjunction with BIAS_MEAS bit for BIAS measurements
#define ADS1299_MUX_MVDD        0x03    ///< MVDD for supply measurement
#define ADS1299_MUX_TEMP        0x04    ///< Temperature sensor
#define ADS1299_MUX_TEST        0x05    ///< Test signal
#define ADS1299_MUX_BIAS_DRP    0x06    ///< BIAS_DRP (positive electrode is the driver)
#define ADS1299_MUX_BIAS_DRN    0x07    ///< BIAS_DRN (negative electrode is the driver)

/// PGA Gain
#define ADS1299_GAIN_1          0x00    ///< Усиление 1
#define ADS1299_GAIN_2          0x00    ///< Усиление 2
#define ADS1299_GAIN_4          0x00    ///< Усиление 4
#define ADS1299_GAIN_6          0x00    ///< Усиление 6
#define ADS1299_GAIN_8          0x00    ///< Усиление 8
#define ADS1299_GAIN_12         0x00    ///< Усиление 12
#define ADS1299_GAIN_24         0x00    ///< Усиление 24

/// Номера каналов
#define ADS1299_CH_1            0       ///< Канал 1 
#define ADS1299_CH_2            1       ///< Канал 2
#define ADS1299_CH_3            2       ///< Канал 3
#define ADS1299_CH_4            3       ///< Канал 4
#define ADS1299_CH_5            4       ///< Канал 5
#define ADS1299_CH_6            5       ///< Канал 6
#define ADS1299_CH_7            6       ///< Канал 7
#define ADS1299_CH_8            7       ///< Канал 8

/// СТРУКТУРЫ
/// Configuration Register 1
__packed typedef struct {
    uint8_t     dr:3;       ///< Output data rate
    uint8_t     bit3:1;     ///< set 0
    uint8_t     bit4:1;     ///< set 1
    uint8_t     clk_en:1;   ///< CLK connetion
    uint8_t     daisy_en:1; ///< Daisy-chain or multiple readback mode
    uint8_t     bit7:1;     ///< set 1
} ads1299_config1_t;

/// Configuration Register 2
__packed typedef struct {
    uint8_t     cal_freq:2; ///< Test signal frequency
    uint8_t     cal_amp:1;  ///< Test signal amplitude
    uint8_t     bit3:1;     ///< set 0
    uint8_t     int_cal:1;  ///< Test source
    uint8_t     bit5:1;     ///< set 0
    uint8_t     bit6:1;     ///< set 1
    uint8_t     bit7:1;     ///< set 1
} ads1299_config2_t;

/// Configuration Register 3
__packed typedef struct {
    uint8_t     bias_stat:1;        ///< BIAS lead-off status (0 - connected)
    uint8_t     bias_loff_sens:1;   ///< BIAS sense function (0 - disabled)
    uint8_t     pd_bias:1;         ///< BIAS buffer power (1 - BIAS buffer is enabled)
    uint8_t     biasref_int:1;      ///< BIASREF signal (0 : BIASREF signal fed externally, 1 : BIASREF signal (AVDD + AVSS) / 2 generated internally)
    uint8_t     bias_meas:1;        ///< BIAS measurement
    uint8_t     bit5:1;             ///< set 1
    uint8_t     bit6:1;             ///< set 1
    uint8_t     pd_refbuf:1;       ///< Power-down reference buffer (1 : Enable internal reference buffer)
} ads1299_config3_t;

/// Configuration Register 4
__packed typedef struct {
    uint8_t     bit0:1;           ///< set 0
    uint8_t     pd_loff_comp:1;   ///< Lead-off comparator power-down (0 : Lead-off comparators disabled)
    uint8_t     bit2:1;           ///< set 0
    uint8_t     single_shot:1;    ///< Single-shot conversion (0 : Continuous conversion mode, 1 : Single-shot mode)
    uint8_t     bit4:1;           ///< set 0
    uint8_t     bit5:1;           ///< set 0
    uint8_t     bit6:1;           ///< set 0
    uint8_t     bit7:1;           ///< set 0
} ads1299_config4_t;

/// Lead-Off Control Register
__packed typedef struct {
    uint8_t     flead_off:2;      ///< Lead-off frequency
    uint8_t     ilead_off:2;      ///< Lead-off current magnitude
    uint8_t     bit4:1;           ///< set 0
    uint8_t     comp_th:3;        ///< Lead-off comparator threshold
} ads1299_loff_t;

/// Individual Channel Settings
__packed typedef struct {
    uint8_t     mux:3;            ///< Channel input
    uint8_t     srb2:1;           ///< SRB2 connection (0: Open, 1: Closed)
    uint8_t     gain:3;           ///< PGA gain
    uint8_t     pd:1;             ///< Power-down (0 : Normal operation, 1 : Channel power-down)
} ads1299_chset_t;

/// General-Purpose I/O
__packed typedef struct {
    uint8_t     ctrl_4:1;         ///< ctrl GPIO4 (0: Output, 1: Input)
    uint8_t     ctrl_3:1;         ///< ctrl GPIO3 (0: Output, 1: Input)
    uint8_t     ctrl_2:1;         ///< ctrl GPIO2 (0: Output, 1: Input)
    uint8_t     ctrl_1:1;         ///< ctrl GPIO1 (0: Output, 1: Input)
    uint8_t     data_4:1;         ///< data GPIO4
    uint8_t     data_3:1;         ///< data GPIO3
    uint8_t     data_2:1;         ///< data GPIO2
    uint8_t     data_1:1;         ///< data GPIO1
} ads1299_gpio_t;

/// Miscellaneous 1 Register
__packed typedef struct {
    uint8_t     bit0:1;           ///< set 0
    uint8_t     bit1:1;           ///< set 0
    uint8_t     bit2:1;           ///< set 0
    uint8_t     bit3:1;           ///< set 0
    uint8_t     bit4:1;           ///< set 0
    uint8_t     srb1:1;           ///< Stimulus, reference, and bias 1 (0 : Switches open, 1 : Switches closed)
    uint8_t     bit6:1;           ///< set 0
    uint8_t     bit7:1;           ///< set 0
} ads1299_misc1_t;

/// конфигурация микросхемы
__packed typedef struct {
    ads1299_config1_t   config1;        ///< Configuration Register 1
    ads1299_config2_t   config2;        ///< Configuration Register 2
    ads1299_config3_t   config3;        ///< Configuration Register 3
    ads1299_loff_t      loff;           ///< Lead-Off Control Register
    ads1299_chset_t     ch1set;         ///< Individual Channel 1 Settings Register
    ads1299_chset_t     ch2set;         ///< Individual Channel 2 Settings Register
    ads1299_chset_t     ch3set;         ///< Individual Channel 3 Settings Register
    ads1299_chset_t     ch4set;         ///< Individual Channel 4 Settings Register
    ads1299_chset_t     ch5set;         ///< Individual Channel 5 Settings Register
    ads1299_chset_t     ch6set;         ///< Individual Channel 6 Settings Register
    ads1299_chset_t     ch7set;         ///< Individual Channel 7 Settings Register
    ads1299_chset_t     ch8set;         ///< Individual Channel 8 Settings Register
    uint8_t             beas_sensp;     ///< Bias Drive Positive Derivation Register
    uint8_t             beas_sensn;     ///< Bias Drive Negative Derivation Register
    uint8_t             loff_sensp;     ///< Positive Signal Lead-Off Detection Register
    uint8_t             loff_sensn;     ///< Negative Signal Lead-Off Detection Register
    uint8_t             loff_flip;      ///< Lead-Off Flip Register
    ads1299_gpio_t      gpio;           ///< General-Purpose I/O Register
    ads1299_misc1_t     misc1;          ///< Miscellaneous 1 Register
    uint8_t             misc2;          ///< Всегда 0
    ads1299_config4_t   config4;        ///< Configuration Register 4
} ads1299_config_t;



/**
 * @brief Дефолтный конфиг
 * 
 * @param config - Указатель на буфер для размещения конфигурации
 * 
 * @return
 * Возвращает дефолтную конфигурацию
*/
void ads1299_def_config(ads1299_config_t *config);


/**
 * @brief Начальная инициализация микросхемы
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param config - Указатель на конфигурацию
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
 * 
 * В процессе начальной инициализации происходит сброс и переход в командный режим работы
 * Если задан файл конфигурации, то происходит загрузка конфигурации
*/
uint16_t ads1299_init(ads129x_handle_t handle, ads1299_config_t *config);


/**
 * @brief Запрос текущей конфигурации
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param config - Указатель на буфер для размещения конфигурации
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1299_get_config(ads129x_handle_t handle, ads1299_config_t *config);


/**
 * @brief Установка новой конфигурации
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param config - Указатель на конфигурацию
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1299_set_config(ads129x_handle_t handle, ads1299_config_t *config);


/**
 * @brief Изменение настроек канала
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param ch_no - Номер канала
 * @param ch_config - Указатель на конфигурацию канала
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1299_set_chcfg(ads129x_handle_t handle, uint8_t ch_no, ads1299_chset_t ch_config);


/**
 * @brief Установка любого регистра в любое значение
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param reg_addr - Адрес регистра
 * @param val - Новое значение регистра
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1299_set_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t val);


/**
 * @brief Чтение любого регистра
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param reg_addr - Адрес регистра
 * @param val - Прочитанное значение регистра
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1299_get_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t *val);


/**
 * @brief Чтение данных АЦП
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param data - Указатель на буфер размещения данных
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1299_get_data(ads129x_handle_t handle, ads129x_data_t *data);



#endif
