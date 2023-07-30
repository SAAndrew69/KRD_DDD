#ifndef ADS1298_H
#define ADS1298_H

/**
 * Описание регистров ADS1298
 * Описание функций для работы с ADS1298
 * 
*/


#include <stdbool.h>
#include <stdint.h>

#include "ads129x.h"


/// РЕГИСТРЫ ADS1298
#define ADS1298_REG_ID          0x00    ///< ID микросхемы
#define ADS1298_REG_CONFIG1     0x01    ///< Конфигурационный регистр 1
#define ADS1298_REG_CONFIG2     0x02    ///< Конфигурационный регистр 2
#define ADS1298_REG_CONFIG3     0x03    ///< Конфигурационный регистр 3
#define ADS1298_REG_LOFF        0x04    ///< Lead-Off Control Register
#define ADS1298_REG_CH1SET      0x05    ///< Индивидуальные настройки канала 1
#define ADS1298_REG_CH2SET      0x06    ///< Индивидуальные настройки канала 2
#define ADS1298_REG_CH3SET      0x07    ///< Индивидуальные настройки канала 3
#define ADS1298_REG_CH4SET      0x08    ///< Индивидуальные настройки канала 4
#define ADS1298_REG_CH5SET      0x09    ///< Индивидуальные настройки канала 5
#define ADS1298_REG_CH6SET      0x0A    ///< Индивидуальные настройки канала 6
#define ADS1298_REG_CH7SET      0x0B    ///< Индивидуальные настройки канала 7
#define ADS1298_REG_CH8SET      0x0C    ///< Индивидуальные настройки канала 8
#define ADS1298_REG_RLD_SENSP   0x0D    ///< Bias Drive Positive Derivation Register
#define ADS1298_REG_RLD_SENSN   0x0E    ///< Bias Drive Negative Derivation Register
#define ADS1298_REG_LOFF_SENSP  0x0F    ///< Positive Signal Lead-Off Detection Register
#define ADS1298_REG_LOFF_SENSN  0x10    ///< Negative Signal Lead-Off Detection Register
#define ADS1298_REG_LOFF_FLIP   0x11    ///< Lead-Off Flip Register
#define ADS1298_REG_LOFF_STATP  0x12    ///< Lead-Off Positive Signal Status Register
#define ADS1298_REG_LOFF_STATN  0x13    ///< Lead-Off Negative Signal Status Register
#define ADS1298_REG_GPIO        0x14    ///< General-Purpose I/O Register
#define ADS1298_REG_PACE        0x15    ///< Pace Detect Register
#define ADS1298_REG_RESP        0x16    ///< Respiration Control Register
#define ADS1298_REG_CONFIG4     0x17    ///< Конфигурационный регистр 4
#define ADS1298_REG_WCT1        0x18    ///< Wilson Central Terminal and Augmented Lead Control Register
#define ADS1298_REG_WCT2        0x19    ///< Wilson Central Terminal Control Register
#define ADS1298_REG_LAST        ADS1298_REG_WCT2 ///< адрес последнего конфигурационного регистра

/// БИТОВЫЕ МАСКИ, ЗНАЧЕНИЯ ПОЛЕЙ
#define ADS1298_ID_MASK         0xFF    ///< Маска для выделения ID микросхемы
#define ADS1298_ID              0x92    ///< ID ADS1298 (8 каналов)

/// For High-Resolution mode, fMOD = fCLK / 4. For low power mode, fMOD = fCLK / 8
#define ADS1298_DR_16KSPS       0x00    ///< fMOD / 16 (HR Mode: 32 kSPS, LP Mode: 16 kSPS)
#define ADS1298_DR_8KSPS        0x01    ///< fMOD / 32 (HR Mode: 16 kSPS, LP Mode: 8 kSPS)
#define ADS1298_DR_4KSPS        0x02    ///< fMOD / 64 (HR Mode: 8 kSPS, LP Mode: 4 kSPS)
#define ADS1298_DR_2KSPS        0x03    ///< fMOD / 128 (HR Mode: 4 kSPS, LP Mode: 2 kSPS)
#define ADS1298_DR_1KSPS        0x04    ///< fMOD / 256 (HR Mode: 2 kSPS, LP Mode: 1 kSPS)
#define ADS1298_DR_500SPS       0x05    ///< fMOD / 512 (HR Mode: 1 kSPS, LP Mode: 500 SPS)
#define ADS1298_DR_250SPS       0x06    ///< fMOD / 1024 (HR Mode: 500 SPS, LP Mode: 250 SPS)

/// High-resolution or low-power mode
#define ADS1298_HR_HR_MODE      0x01    ///< High-resolution mode
#define ADS1298_HR_LP_MODE      0x00    ///< Low-power resolution mode

#define ADS1298_TEST_FREQ_1     0x00    ///< Test signal frequency: fCLK / 2^21
#define ADS1298_TEST_FREQ_2     0x01    ///< Test signal frequency: fCLK / 2^20
#define ADS1298_TEST_FREQ_3     0x03    ///< Test signal frequency: DC

/// Lead-off comparator threshold (Comparator positive side)
#define ADS1298_COMP_THP_95     0x00    ///< 95%
#define ADS1298_COMP_THP_92     0x01    ///< 92.5%
#define ADS1298_COMP_THP_90     0x02    ///< 90%
#define ADS1298_COMP_THP_87     0x03    ///< 87.5%
#define ADS1298_COMP_THP_85     0x04    ///< 85%
#define ADS1298_COMP_THP_80     0x05    ///< 80%
#define ADS1298_COMP_THP_75     0x06    ///< 75%
#define ADS1298_COMP_THP_70     0x07    ///< 70%
/// Lead-off comparator threshold (Comparator negative side)
#define ADS1298_COMP_THN_5      0x00    ///< 5%
#define ADS1298_COMP_THN_7      0x01    ///< 7.5%
#define ADS1298_COMP_THN_10     0x02    ///< 10%
#define ADS1298_COMP_THN_12     0x03    ///< 12.5%
#define ADS1298_COMP_THN_15     0x04    ///< 15%
#define ADS1298_COMP_THN_20     0x05    ///< 20%
#define ADS1298_COMP_THN_25     0x06    ///< 25%
#define ADS1298_COMP_THN_30     0x07    ///< 30%

/// Lead-off current magnitude (determine the magnitude of current for the current lead-off mode)
#define ADS1298_ILEAD_OFF_6N    0x00    ///< 6nA
#define ADS1298_ILEAD_OFF_12N   0x01    ///< 12nA
#define ADS1298_ILEAD_OFF_18N   0x02    ///< 18nA
#define ADS1298_ILEAD_OFF_24N   0x03    ///< 24nA

/// Lead-off frequency (determine the frequency of lead-off detect for each channel)
#define ADS1298_FLEAD_OFF_LOFF  0x00    ///< When any bits of the LOFF_SENSP or LOFF_SENSN registers are turned on, make sure that FLEAD[1:0] are either set to 01 or 11
#define ADS1298_FLEAD_OFF_AC    0x01    ///< AC lead-off detection at fDR / 4
#define ADS1298_FLEAD_OFF_DC    0x03    ///< DC lead-off detection turned on

/// Individual Channel Settings
#define ADS1298_MUX_NORMAL      0x00    ///< Normal electrode input
#define ADS1298_MUX_SHORT       0x01    ///< Input shorted (for offset or noise measurements)
#define ADS1298_MUX_CONJ        0x02    ///< Used in conjunction with RLD_MEAS bit for RLD
#define ADS1298_MUX_MVDD        0x03    ///< MVDD for supply measurement
#define ADS1298_MUX_TEMP        0x04    ///< Temperature sensor
#define ADS1298_MUX_TEST        0x05    ///< Test signal
#define ADS1298_MUX_RLD_DRP     0x06    ///< RLD_DRP (positive electrode is the driver)
#define ADS1298_MUX_RLD_DRN     0x07    ///< RLD_DRN  (negative electrode is the driver)

/// PGA Gain
#define ADS1298_GAIN_6          0x00    ///< Усиление 6
#define ADS1298_GAIN_1          0x01    ///< Усиление 1
#define ADS1298_GAIN_2          0x02    ///< Усиление 2
#define ADS1298_GAIN_3          0x03    ///< Усиление 3
#define ADS1298_GAIN_4          0x04    ///< Усиление 4
#define ADS1298_GAIN_8          0x05    ///< Усиление 8
#define ADS1298_GAIN_12         0x06    ///< Усиление 12

/// Cahnnel Power-down bit
#define ADS1298_PD_POWERDOWN    0x01    ///< Канал выключен
#define ADS1298_PD_NORMAL       0x00    ///< Канал включен

/// Refference buffer power-down bit
#define ADS1298_PD_REF_POWERDOWN 0x00   ///< Внутренний референс отключен
#define ADS1298_PD_REF_ON       0x01    ///< Внутренний референс подключен

/// Номера каналов
#define ADS1298_CH_1            0       ///< Канал 1 
#define ADS1298_CH_2            1       ///< Канал 2
#define ADS1298_CH_3            2       ///< Канал 3
#define ADS1298_CH_4            3       ///< Канал 4
#define ADS1298_CH_5            4       ///< Канал 5
#define ADS1298_CH_6            5       ///< Канал 6
#define ADS1298_CH_7            6       ///< Канал 7
#define ADS1298_CH_8            7       ///< Канал 8

/// Respiration modulation frequency
#define ADS1298_RESP_FREQ_64K   0x00    ///< 64 kHz modulation clock
#define ADS1298_RESP_FREQ_32K   0x01    ///< 32 kHz modulation clock
#define ADS1298_RESP_FREQ_16K   0x02    ///< 16kHz square wave on GPIO3 and GPIO04. Output on GPIO4 is 180 degree out of phase with GPIO3
#define ADS1298_RESP_FREQ_8K    0x03    ///< 8kHz square wave on GPIO3 and GPIO04. Output on GPIO4 is 180 degree out of phase with GPIO3
#define ADS1298_RESP_FREQ_4K    0x04    ///< 4kHz square wave on GPIO3 and GPIO04. Output on GPIO4 is 180 degree out of phase with GPIO3
#define ADS1298_RESP_FREQ_2K    0x05    ///< 2kHz square wave on GPIO3 and GPIO04. Output on GPIO4 is 180 degree out of phase with GPIO3
#define ADS1298_RESP_FREQ_1K    0x06    ///< 1kHz square wave on GPIO3 and GPIO04. Output on GPIO4 is 180 degree out of phase with GPIO3
#define ADS1298_RESP_FREQ_500   0x07    ///< 500Hz square wave on GPIO3 and GPIO04. Output on GPIO4 is 180 degree out of phase with GPIO3

/// Pace even channels
#define ADS1298_PACE_EVEN_CH2   0x00    ///< Channel 2
#define ADS1298_PACE_EVEN_CH4   0x01    ///< Channel 4
#define ADS1298_PACE_EVEN_CH6   0x02    ///< Channel 6
#define ADS1298_PACE_EVEN_CH8   0x03    ///< Channel 8

/// Pace odd channels
#define ADS1298_PACE_ODD_CH1    0x00    ///< Channel 1
#define ADS1298_PACE_ODD_CH3    0x01    ///< Channel 3
#define ADS1298_PACE_ODD_CH5    0x02    ///< Channel 5
#define ADS1298_PACE_ODD_CH7    0x03    ///< Channel 7

/// Respiration phase
#define ADS1298_RESP_PH_22_5    0x00    ///< 22.5 градуса
#define ADS1298_RESP_PH_45      0x01    ///< 45 градусов
#define ADS1298_RESP_PH_67_5    0x02    ///< 67.5 градуса
#define ADS1298_RESP_PH_90      0x03    ///< 90 градусов
#define ADS1298_RESP_PH_112_5   0x04    ///< 112.5 градуса
#define ADS1298_RESP_PH_135     0x05    ///< 135 градусов
#define ADS1298_RESP_PH_157_5   0x06    ///< 157.5 градуса

/// Respiration control
#define ADS1298_RESP_CTRL_OFF   0x00    ///< No respiration
#define ADS1298_RESP_CTRL_EXT   0x01    ///< External respiration
#define ADS1298_RESP_CTRL_INT   0x02    ///< Internal respiration with internal signals
#define ADS1298_RESP_CTRL_USER  0x03    ///< Internal respiration with user-generated signals

/// WCT Amplifier A channel selection, typically connected to RA electrode
#define ADS1298_WCTA_CH1P       0x00    ///< Channel 1 positive input connected to WCTA amplifier
#define ADS1298_WCTA_CH1N       0x01    ///< Channel 1 negative input connected to WCTA amplifier
#define ADS1298_WCTA_CH2P       0x02    ///< Channel 2 positive input connected to WCTA amplifier
#define ADS1298_WCTA_CH2N       0x03    ///< Channel 2 negative input connected to WCTA amplifier
#define ADS1298_WCTA_CH3P       0x04    ///< Channel 3 positive input connected to WCTA amplifier
#define ADS1298_WCTA_CH3N       0x05    ///< Channel 3 negative input connected to WCTA amplifier
#define ADS1298_WCTA_CH4P       0x06    ///< Channel 4 positive input connected to WCTA amplifier
#define ADS1298_WCTA_CH4N       0x07    ///< Channel 4 negative input connected to WCTA amplifier

/// WCT Amplifier B channel selection, typically connected to LA electrode
#define ADS1298_WCTB_CH1P       0x00    ///< Channel 1 positive input connected to WCTB amplifier
#define ADS1298_WCTB_CH1N       0x01    ///< Channel 1 negative input connected to WCTB amplifier
#define ADS1298_WCTB_CH2P       0x02    ///< Channel 2 positive input connected to WCTB amplifier
#define ADS1298_WCTB_CH2N       0x03    ///< Channel 2 negative input connected to WCTB amplifier
#define ADS1298_WCTB_CH3P       0x04    ///< Channel 3 positive input connected to WCTB amplifier
#define ADS1298_WCTB_CH3N       0x05    ///< Channel 3 negative input connected to WCTB amplifier
#define ADS1298_WCTB_CH4P       0x06    ///< Channel 4 positive input connected to WCTB amplifier
#define ADS1298_WCTB_CH4N       0x07    ///< Channel 4 negative input connected to WCTB amplifier

/// WCT Amplifier C channel selection, typically connected to LL electrode
#define ADS1298_WCTC_CH1P       0x00    ///< Channel 1 positive input connected to WCTC amplifier
#define ADS1298_WCTC_CH1N       0x01    ///< Channel 1 negative input connected to WCTC amplifier
#define ADS1298_WCTC_CH2P       0x02    ///< Channel 2 positive input connected to WCTC amplifier
#define ADS1298_WCTC_CH2N       0x03    ///< Channel 2 negative input connected to WCTC amplifier
#define ADS1298_WCTC_CH3P       0x04    ///< Channel 3 positive input connected to WCTC amplifier
#define ADS1298_WCTC_CH3N       0x05    ///< Channel 3 negative input connected to WCTC amplifier
#define ADS1298_WCTC_CH4P       0x06    ///< Channel 4 positive input connected to WCTC amplifier
#define ADS1298_WCTC_CH4N       0x07    ///< Channel 4 negative input connected to WCTC amplifier

/// СТРУКТУРЫ
/// Configuration Register 1
__packed typedef struct {
    uint8_t     dr:3;       ///< Output data rate
    uint8_t     bit3:1;     ///< Always write 0h
    uint8_t     bit4:1;     ///< Always write 0h
    uint8_t     clk_en:1;   ///< CLK connetion
    uint8_t     daisy_en:1; ///< Daisy-chain or multiple readback mode
    uint8_t     hr:1;       ///< High-resolution or low-power mode
} ads1298_config1_t;

/// Configuration Register 2
__packed typedef struct {
    uint8_t     test_freq:2; ///< Test signal frequency
    uint8_t     test_amp:1; ///< Test signal amplitude (0 = 1 × –(VREFP – VREFN) / 2400 V, 1 = 2 × –(VREFP – VREFN) / 2400 V)
    uint8_t     bit3:1;     ///< Always write 0h
    uint8_t     int_test:1; ///< Test source (0 = Test signals are driven externally, 1 = Test signals are generated internally)
    uint8_t     wct_chop:1; ///< WCT chopping scheme (0 = Chopping frequency varies, 1 = Chopping frequency constant at fMOD / 16)
    uint8_t     bit6:1;     ///< Always write 0h
    uint8_t     bit7:1;     ///< Always write 0h
} ads1298_config2_t;

/// Configuration Register 3
__packed typedef struct {
    uint8_t     rld_stat:1;        ///< RLD lead-off status (0 - connected)
    uint8_t     rld_loff_sens:1;   ///< RLD sense function (0 = RLD sense is disabled)
    uint8_t     pd_rld:1;          ///< RLD buffer power (1 = RLD buffer is enabled)
    uint8_t     rldref_int:1;      ///< RLDREF signal signal (0 = RLDREF signal fed externally, 1 = RLDREF signal (AVDD – AVSS) / 2 generated internally)
    uint8_t     rld_meas:1;        ///< RLD measurement (0 = Open, 1 = RLD_IN signal is routed to the channel that has the MUX_Setting 010 (VREF))
    uint8_t     vref_4v:1;         ///< Reference voltage (0 = VREFP is set to 2.4 V, 1 = VREFP is set to 4 V (use only with a 5-V analog supply))
    uint8_t     bit6:1;            ///< set 1
    uint8_t     pd_refbuf:1;       ///< Power-down reference buffer (1 : Enable internal reference buffer)
} ads1298_config3_t;

/// Configuration Register 4
__packed typedef struct {
    uint8_t     bit0:1;           ///< set 0
    uint8_t     pd_loff_comp:1;   ///< Lead-off comparator power-down (0 = Lead-off comparators disabled)
    uint8_t     wct_rld:1;        ///< Connects the WCT to the RLD (0 = WCT to RLD connection off)
    uint8_t     single_shot:1;    ///< Single-shot conversion (0 : Continuous conversion mode, 1 : Single-shot mode)
    uint8_t     bit4:1;           ///< set 0
    uint8_t     resp_freq:3;      ///< Respiration modulation frequency
} ads1298_config4_t;

/// Lead-Off Control Register
__packed typedef struct {
    uint8_t     flead_off:2;      ///< Lead-off frequency
    uint8_t     ilead_off:2;      ///< Lead-off current magnitude
    uint8_t     vlead_off_en:1;   ///< Lead-off detection mode (0 = Current source mode lead-off)
    uint8_t     comp_th:3;        ///< Lead-off comparator threshold
} ads1298_loff_t;

/// Individual Channel Settings
__packed typedef struct {
    uint8_t     mux:3;            ///< Channel input
    uint8_t     bit3:1;           ///< set 0
    uint8_t     gain:3;           ///< PGA gain
    uint8_t     pd:1;             ///< Power-down (0 : Normal operation, 1 : Channel power-down)
} ads1298_chset_t;

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
} ads1298_gpio_t;

/// Pace Detect Register
__packed typedef struct {
    uint8_t     pd_pace:1;        ///< Pace detect buffer (0 = Pace detect buffer turned off)
    uint8_t     paceo:1;          ///< Pace odd channels
    uint8_t     pacee:2;          ///< Pace even channels
    uint8_t     bit7:3;           ///< set 0
} ads1298_pace_t;

/// Respiration Control Register
__packed typedef struct {
    uint8_t     resp_ctrl:2;       ///< Respiration control
    uint8_t     resp_ph:2;         ///< Respiration phase
    uint8_t     reserve:1;         ///< Always write 1h
    uint8_t     resp_mod_en1:1;    ///< Enables respiration modulation circuitry (ADS129xR only, for ADS129x always write 0)
    uint8_t     resp_demod_en1:1;  ///< Enables respiration demodulation circuitry (ADS129xR only, for ADS129x always write 0)
} ads1298_resp_t;

/// Wilson Central Terminal and Augmented Lead Control
__packed typedef struct {
    uint8_t     wcta:3;            ///< WCT Amplifier A channel selection, typically connected to RA electrode
    uint8_t     pd_wcta:1;         ///< Power-down WCTA (0 = Powered down)
    uint8_t     avr_ch4:1;         ///< Enable (WCTB + WCTC)/2 to the negative input of channel 4
    uint8_t     avr_ch7:1;         ///< Enable (WCTB + WCTC)/2 to the negative input of channel 7
    uint8_t     avl_ch5:1;         ///< Enable (WCTA + WCTC)/2 to the negative input of channel 5
    uint8_t     avf_ch6:1;         ///< Enable (WCTA + WCTB)/2 to the negative input of channel 6
} ads1298_wct1_t;

/// Wilson Central Terminal Control
__packed typedef struct {
    uint8_t     wctc:3;            ///< Power-down WCTC (0 = Powered down)
    uint8_t     wctb:3;            ///< Power-down WCTB (0 = Powered down)
    uint8_t     pd_wctb:1;         ///< WCT amplifier B channel selection, typically connected to LA electrode
    uint8_t     pd_wctc:1;         ///< WCT amplifier C channel selection, typically connected to LL electrode
} ads1298_wct2_t;


/// конфигурация микросхемы
__packed typedef struct {
    ads1298_config1_t   config1;        ///< Configuration Register 1
    ads1298_config2_t   config2;        ///< Configuration Register 2
    ads1298_config3_t   config3;        ///< Configuration Register 3
    ads1298_loff_t      loff;           ///< Lead-Off Control Register
    ads1298_chset_t     ch1set;         ///< Individual Channel 1 Settings Register
    ads1298_chset_t     ch2set;         ///< Individual Channel 2 Settings Register
    ads1298_chset_t     ch3set;         ///< Individual Channel 3 Settings Register
    ads1298_chset_t     ch4set;         ///< Individual Channel 4 Settings Register
    ads1298_chset_t     ch5set;         ///< Individual Channel 5 Settings Register
    ads1298_chset_t     ch6set;         ///< Individual Channel 6 Settings Register
    ads1298_chset_t     ch7set;         ///< Individual Channel 7 Settings Register
    ads1298_chset_t     ch8set;         ///< Individual Channel 8 Settings Register
    uint8_t             rld_sensp;      ///< Rld Drive Positive Derivation Register
    uint8_t             rld_sensn;      ///< Rld Drive Negative Derivation Register
    uint8_t             loff_sensp;     ///< Positive Signal Lead-Off Detection Register
    uint8_t             loff_sensn;     ///< Negative Signal Lead-Off Detection Register
    uint8_t             loff_flip;      ///< Lead-Off Flip Register
    ads1298_gpio_t      gpio;           ///< General-Purpose I/O Register
    ads1298_pace_t      pace;           ///< Pace Detect Register
    ads1298_resp_t      resp;           ///< Respiration Control Register
    ads1298_config4_t   config4;        ///< Configuration Register 4
    ads1298_wct1_t      wct1;           ///< Wilson Central Terminal and Augmented Lead Control
    ads1298_wct2_t      wct2;           ///< Wilson Central Terminal Control
} ads1298_config_t;



/**
 * @brief Дефолтный конфиг
 * 
 * @param config - Указатель на буфер для размещения конфигурации
 * 
 * @return
 * Возвращает дефолтную конфигурацию
*/
void ads1298_def_config(ads1298_config_t *config);


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
uint16_t ads1298_init(ads129x_handle_t handle, ads1298_config_t *config);


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
uint16_t ads1298_get_config(ads129x_handle_t handle, ads1298_config_t *config);


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
uint16_t ads1298_set_config(ads129x_handle_t handle, ads1298_config_t *config);


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
uint16_t ads1298_set_chcfg(ads129x_handle_t handle, uint8_t ch_no, ads1298_chset_t ch_config);


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
uint16_t ads1298_set_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t val);


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
uint16_t ads1298_get_reg(ads129x_handle_t handle, uint8_t reg_addr, uint8_t *val);


/**
 * @brief Чтение нескольких регистров
 * 
 * @param handle - Хендл микросхемы на шине SPI
 * @param reg_addr - Адрес регистра
 * @param cnt - Число регистров для чтения
 * @param buff - Буфер для чтения
 * 
 * @return
 *  ERR_NOERROR - если ошибок нет
 *  ERR_INVALID_PARAMETR - в случае ошибки во входных параметрах
 *  ERR_NOT_INITED: интерфейс SPI не инициализирован
 *  ERR_TIMEOUT: таймаут ожидания доступа к шине SPI
*/
uint16_t ads1298_get_regs(ads129x_handle_t handle, uint8_t reg_addr, uint8_t cnt, uint8_t *buff);


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
uint16_t ads1298_get_data(ads129x_handle_t handle, ads129x_data_t *data);



#endif
