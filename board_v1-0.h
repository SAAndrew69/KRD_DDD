#ifndef BOARD_V1_0_H
#define BOARD_V1_0_H

#include "settings.h"

/*

*/

#if(BOARD_V1_0)

#define GET_DEVICE_ID()   (*((uint64_t*) NRF_FICR->DEVICEADDR)) // UUID процессора

// приоритет прерываний от GPIOE
#define GPIOTE_IRQ_PRIORITY     7
/* распределение каналов прерываний GPIOTE (всего их 8)
0 - ADS129X


*/
#define GPIOTE_CH_ADS129X      0
#define GPIOTE_INT_ADS129X     GPIOTE_INTENSET_IN0_Msk


// GPIOTE PORT >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define INPUTS_INT_INIT()         do{\
                                    NRF_GPIOTE->INTENCLR = (GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos); \
                                    NRF_P0->DETECTMODE = (GPIO_DETECTMODE_DETECTMODE_LDETECT << GPIO_DETECTMODE_DETECTMODE_Pos); \
                                    NRF_P1->DETECTMODE = (GPIO_DETECTMODE_DETECTMODE_LDETECT << GPIO_DETECTMODE_DETECTMODE_Pos); \
                                    NRF_GPIOTE->EVENTS_PORT = 0; \
                                  }while(0)
#define INPUTS_INT_DISABLE()     (NRF_GPIOTE->INTENCLR = (GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos))
#define INPUTS_INT_ENABLE()      do{\
                                    NRF_GPIOTE->EVENTS_PORT = 0; \
                                    NRF_GPIOTE->INTENSET = (GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos);\
                                 }while(0)
#define IS_INPUTS_INT()          (NRF_GPIOTE->EVENTS_PORT != 0) // true, если было прерывание
// GPIOTE PORT <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


// SPI ADC >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define SPIM3_PRIORITY          6 // приоритет прерывания для SPIM3
#define SPIM3                   NRF_SPIM3 // ADS129X два корпуса
#define SPIM3_IRQHandler        SPIM3_IRQHandler
#define SPIM3_IRQn              SPIM3_IRQn
#define SPIM3_FREQUENCY         SPIM_FREQUENCY_FREQUENCY_M1 //(unsigned int)0x14000000UL /*!< 32 Mbps */
#define SPIM3_MOSI_PIN          30 // P0.30
#define SPIM3_MISO_PIN          (32+13) // P1.13
#define SPIM3_SCK_PIN           29 // P0.29
#define SPIM3_CS0_PIN           (32+4) // P1.04
#define SPIM3_CS1_PIN           3 // P0.03
#define SPIM3_PINS_INIT()       do{ \
                                  NRF_P1->OUTCLR = (1UL << (SPIM3_MISO_PIN - 32)); \
                                  NRF_P1->PIN_CNF[SPIM3_MISO_PIN - 32] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos); \
                                  NRF_P0->DIRSET = (1UL << SPIM3_SCK_PIN);  \
                                  NRF_P0->DIRSET = (unsigned int)(1UL << SPIM3_MOSI_PIN); \
                                  NRF_P1->OUTSET = (1UL << (SPIM3_CS0_PIN - 32)); \
                                  NRF_P0->OUTSET = (1UL << SPIM3_CS1_PIN); \
                                  NRF_P1->DIRSET = (1UL << (SPIM3_CS0_PIN - 32)); \
                                  NRF_P0->DIRSET = (1UL << SPIM3_CS1_PIN); \
                                }while(0)

#define ADS129X_CS0_PIN         SPIM3_CS0_PIN
#define ADS129X_CS1_PIN         SPIM3_CS1_PIN
#define ADS129X_RESET_PIN       32 // P1.00 (активный 0) сброс
#define ADS129X_PWDN_PIN        (32+11) // P1.11 (активный 0) переход в режим энергосбереженеия
#define ADS129X_RDY_PIN         (32+10) // P1.10 (активный 0) готовность данных
#define ADS129X_PWR_PIN         (32+9) // P1.09 (активный 1) управлением питанием

#define ADS129X_RESET_ON()      do{NRF_P1->OUTCLR = (1UL << (ADS129X_RESET_PIN - 32)); NRF_P1->DIRSET = (1UL << (ADS129X_RESET_PIN - 32));}while(0)
#define ADS129X_RESET_OFF()     do{NRF_P1->OUTSET = (1UL << (ADS129X_RESET_PIN - 32)); NRF_P1->DIRSET = (1UL << (ADS129X_RESET_PIN - 32));}while(0)
#define ADS129X_PWDN_ON()       do{NRF_P1->OUTCLR = (1UL << (ADS129X_PWDN_PIN - 32)); NRF_P1->DIRSET = (1UL << (ADS129X_PWDN_PIN - 32));}while(0)
#define ADS129X_PWDN_OFF()      do{NRF_P1->OUTSET = (1UL << (ADS129X_PWDN_PIN - 32)); NRF_P1->DIRSET = (1UL << (ADS129X_PWDN_PIN - 32));}while(0)
#define ANA_PWR_ON()            do{NRF_P1->OUTSET = (1UL << (ADS129X_PWR_PIN - 32)); NRF_P1->DIRSET = (1UL << (ADS129X_PWR_PIN - 32));}while(0)
#define ANA_PWR_OFF()           do{NRF_P1->OUTCLR = (1UL << (ADS129X_PWR_PIN - 32)); NRF_P1->DIRSET = (1UL << (ADS129X_PWR_PIN - 32));}while(0)

#define ADS129X_START_PIN       (5) // P0.05 Управление запуском измерений (активный 1)
#define ADS129X_START()         do{NRF_P0->OUTSET = (1UL << (ADS129X_START_PIN)); NRF_P0->DIRSET = (1UL << (ADS129X_START_PIN));}while(0)
#define ADS129X_STOP()          do{NRF_P0->OUTCLR = (1UL << (ADS129X_START_PIN)); NRF_P0->DIRSET = (1UL << (ADS129X_START_PIN));}while(0)

#define ADS129X_INT_ENABLE()      (NRF_GPIOTE->INTENSET = GPIOTE_INT_ADS129X)
#define ADS129X_INT_DISABLE()     (NRF_GPIOTE->INTENCLR = GPIOTE_INT_ADS129X)
#define ADS129X_INT_IS_ENABLED()  (NRF_GPIOTE->INTENSET & GPIOTE_INT_ADS129X)
#define ADS129X_RDY_INIT()        do{ \
                                      NRF_P1->DIRCLR = (1UL << (ADS129X_RDY_PIN - 32)); \
                                      NRF_P1->PIN_CNF[ADS129X_RDY_PIN - 32] = ((GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos ) | (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)); \
                                      NRF_GPIOTE->CONFIG[GPIOTE_CH_ADS129X] = ((GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) \
                                      | (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos) \
                                      | (ADS129X_RDY_PIN << GPIOTE_CONFIG_PSEL_Pos)); \
                                      NRF_GPIOTE->EVENTS_IN[GPIOTE_CH_ADS129X] = 0; \
                                  }while(0)

#define ADS129X_RDY_DEINIT()      do{ \
                                      NRF_GPIOTE->EVENTS_IN[GPIOTE_CH_ADS129X] = 0; \
                                      NRF_P1->PIN_CNF[ADS129X_RDY_PIN-32] = 0; \
                                      NRF_GPIOTE->CONFIG[GPIOTE_CH_ADS129X] = ((GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) \
                                      | (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos) \
                                      | (ADS129X_RDY_PIN << GPIOTE_CONFIG_PSEL_Pos)); \
                                  }while(0)
// SPI ADC  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                                


#endif // BOARD_V1_0
#endif

