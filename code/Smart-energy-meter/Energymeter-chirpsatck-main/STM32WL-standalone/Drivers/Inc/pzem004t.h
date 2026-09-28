/*
 * pzem004t.h
 *
 *  PZEM-004T V4.0 (100A) Energy Meter Driver Header
 *  USART2 @ 9600 baud, 8N1 — PA2=TX, PA3=RX (STM32WL Wio-E5 mini)
 *
 *  Created on: Sep 2025
 *      Author: Antz IOT
 */

#ifndef INC_PZEM004T_H_
#define INC_PZEM004T_H_

#include <stdint.h>
#include "stm32wlxx_hal.h"

/* -------------------------------------------------------------------------- */
/*  Status codes                                                               */
/* -------------------------------------------------------------------------- */
typedef enum
{
    PZEM_OK            = 0,
    PZEM_ERROR_TIMEOUT = 1,
    PZEM_ERROR_CRC     = 2
} pzem_status_t;

/* -------------------------------------------------------------------------- */
/*  Measurement data (all scaled integers — no float needed)                  */
/*                                                                             */
/*  voltage_dV    : Voltage in units of 0.1 V  (e.g. 2300 = 230.0 V)         */
/*  current_mA    : Current in units of 1 mA   (e.g. 5000 = 5.000 A)         */
/*  power_dW      : Active power in units of 0.1 W (e.g. 11500 = 1150.0 W)   */
/*  energy_Wh     : Energy counter in Wh        (e.g. 1234 = 1234 Wh)        */
/*  frequency_dHz : Frequency in units of 0.1 Hz(e.g. 500  = 50.0 Hz)        */
/*  power_factor  : PF in units of 0.01         (e.g. 98   = 0.98)           */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint16_t voltage_dV;      /* 0.1 V  resolution */
    uint32_t current_mA;      /* 1 mA   resolution */
    uint32_t power_dW;        /* 0.1 W  resolution */
    uint32_t energy_Wh;       /* 1 Wh   resolution */
    uint16_t frequency_dHz;   /* 0.1 Hz resolution */
    uint16_t power_factor;    /* 0.01   resolution */
} pzem_data_t;

/* -------------------------------------------------------------------------- */
/*  USART2 handle (shared for debug use if needed)                            */
/* -------------------------------------------------------------------------- */
extern UART_HandleTypeDef hlpuart1;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */
void          PZEM_UART_Init  (void);
void          PZEM_UART_DeInit(void);
pzem_status_t PZEM_Read       (pzem_data_t *data);
pzem_status_t PZEM_ReadOnce   (pzem_data_t *data);

#endif /* INC_PZEM004T_H_ */
