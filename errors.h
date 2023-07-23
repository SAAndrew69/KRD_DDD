#ifndef ERRORS_H
#define ERRORS_H
/*

*/

#define ERROR_CHECK(ERR_CODE)                           \
    do                                                      \
    {                                                       \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);         \
        if (LOCAL_ERR_CODE != NRF_SUCCESS)                  \
        {                                                   \
            return(LOCAL_ERR_CODE);       \
        }                                                   \
    } while (0)


#define ERR_NOERROR             0x0000    // ошибок нет

// маски кодов ошибок
#define ERR_BUSY                0x0001    // устройство занято или отсутствует
#define ERR_TWI_ERROR           0x0002    // ошибка на шине TWI
#define ERR_READ                0x0003    // ошибка при чтении
#define ERR_WRITE               0x0004    // ошибка при записи
#define ERR_FIFO_OVF            0x0005    // переполнение буфера фифо
#define ERR_UNKNOWN             0x0006    // неизвестная ошибка
#define ERR_SOFTDEVICE          0x0007    // какая-то ошибка софтдевайса
#define ERR_FIFO_INIT           0x0008    // ошибка, фифо не было проинициализировано
#define ERR_INVALID_PARAMETR    0x0009    // неправильных входной параметр
#define ERR_BUFF_OVF            0x000A    // переполнение буфера
#define ERR_DATA_STRUCT         0x000B    // ошибка структуры данных
#define ERR_DATA                0x000C    // ошибка в данных
#define ERR_NO_SPACE            0x000D    // ошибка: нет свободного места (пример, при инициализации нового интерфейса сверх максимально установленного значения)
#define ERR_NOT_INITED          0x000E    // устройство или модуль не прошло инициализацию
#define ERR_CRC                 0x000F    // ошибка CRC
#define ERR_TIMEOUT             0x0010    // срабатывание таймаута
#define ERR_CMD                 0x0011    // команда не была выполнена
#define ERR_WDT                 0x0012    // сброс по WDT
#define ERR_PWR_ON              0x0013    // сброс по включению питания
#define ERR_RESET               0x0014    // сброс по входу RESET
#define ERR_OUT_OF_MEMORY       0x0015    // недостаточно памяти
#define ERR_ALREADY_INITED      0x0016    // уже инициализировано
#define ERR_INVALID_STATE       0x0017    // неверное состояние
#define ERR_SOFTRESET           0x0018    // программный сброс
#define ERR_DISABLED            0x0019    // устройство или команда запрещены

#endif
