#ifndef CMD_H
#define CMD_H

/*
Модуль обработчика команд управления
*/

#include <stdbool.h>
#include <stdint.h>



#ifndef CMD_LEN_MAX
#define CMD_LEN_MAX           128 ///< максимальная длина любых данных, которые могут быть переданы одной командой (вместе со всеми служебными полями)
#endif


/// @brief Команды управления (все команды состоят из одного байта)
typedef enum
{
    CMD_CMD_STATUS  = 's', ///< Выдача идентификационной информации об устройстве
    CMD_CMD_START   = 'b', ///< Запуск процесса измерения
    CMD_CMD_STOP    = 'e', ///< Останов процесса измерения
    CMD_CMD_IND     = 'f', ///< Включение/выключение индикации
    CMD_CMD_REBOOT  = 'r', ///< Перезагрузка устройства
    CMD_CMD_MAC     = 'm', ///< Выдача МАС-адреса устройства (текстом в шестнадцатиричном виде)
    CMD_CMD_TIME    = 't', ///< Запрос текущего времени
    CMD_CMD_PDOWN   = 'o', ///< Принудительное выключение устройства
    CMD_CMD_CONSUM  = 'p', ///< Выдать текущее потребление устройства
    CMD_CMD_IND_ON  = 'l', ///< Зажечь светодиод индикации состояния
    CMD_CMD_BNT     = 'k', ///< Выдать текущее состояние кнопки включения
    CMD_CMD_DELTA   = 'd', ///< Сдвинуть время на дельту
    CMD_CMD_LOG     = 'n', ///< Выдать лог работы
    CMD_CMD_GAUGE   = 'g', ///< Выдать информацию по Gas gauge
    CMD_CMD_BAT_ON  = 'j', ///< Зажечь сегмент индикатора уровня заряда
    CMD_CMD_VIBRO   = 'v', ///< Инициировать виброиндикатор
    CMD_CMD_GET_CAL = 'y', ///< Считать калибровочный коэффициент по каналу
    CMD_CMD_SET_CAL = 'z', ///< Записать калибровочный коффициент по каналу
    CMD_CMD_SHOT    = 'c', ///< Единичный отсчет АЦП
    CMD_CMD_RSSI    = 'i', ///< Выдать уровнь сигнала BLE
    CMD_CMD_FW      = 'u', ///< Перейти в режим обновления прошивки
    CMD_CMD_GET_CFG = 'G', ///< Запрос конфига (формат: G,n)
    CMD_CMD_SET_CFG = 'S', ///< Установка нового конфига (формат: S,n,rrvv,....,rrvv где n - номер АЦП (0 или 1), rrvv - uint16_t, где rr - адрес регистра, vv - значение регистра))
} cmd_cmd_e;

















#endif
