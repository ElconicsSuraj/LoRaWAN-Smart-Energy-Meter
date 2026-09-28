/*
 * pzem004t.c
 *
 *  PZEM-004T V4.0 (100A) Energy Meter Driver
 *  Interface: USART2 @ 9600 baud, 8N1 (Modbus RTU)
 *  STM32WL Wio-E5 mini pins: PA2 = USART2_TX, PA3 = USART2_RX
 *
 *  PZEM-004T V4.0 register map (Modbus RTU, slave address 0xF8 broadcast):
 *    0x0000 - Voltage (0.1 V resolution, divide by 10)
 *    0x0001 - Current low word (0.001 A resolution)
 *    0x0002 - Current high word
 *    0x0003 - Power low word  (0.1 W resolution)
 *    0x0004 - Power high word
 *    0x0005 - Energy low word (1 Wh resolution)
 *    0x0006 - Energy high word
 *    0x0007 - Frequency      (0.1 Hz resolution)
 *    0x0008 - Power Factor   (0.01 resolution)
 *    0x0009 - Alarm status
 *
 *  Created on: Sep 2025
 *      Author: Antz IOT
 */

#include "pzem004t.h"
#include "main.h"
#include "usart.h"
#include "application_logic.h"
#include <string.h>

#define PZEM_SLAVE_ADDR     0x01   /* Verified on ESP32 */
#define PZEM_RESPONSE_LEN   25U

static uint8_t pzem_rx_buf[PZEM_RESPONSE_LEN];

static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

static uint8_t pzem_crc_valid(const uint8_t *buf, uint16_t len)
{
    if (len < 2) return 0;
    uint16_t received_crc = ((uint16_t)buf[len - 1] << 8) | buf[len - 2];
    uint16_t calc_crc     = modbus_crc16(buf, len - 2);
    return (received_crc == calc_crc) ? 1 : 0;
}

void PZEM_UART_Init(void)
{
    MX_USART2_UART_Init();
}

void PZEM_UART_DeInit(void)
{
    /* Keep UART2 initialized to prevent line floating */
}

static inline uint16_t pzem_reg(const uint8_t *resp, int n)
{
    return ((uint16_t)resp[3 + 2 * n] << 8) | resp[4 + 2 * n];
}

pzem_status_t PZEM_Read(pzem_data_t *data)
{
    if (data == NULL) return PZEM_ERROR_CRC;

    uint8_t req[8] = { PZEM_SLAVE_ADDR, 0x04, 0x00, 0x00, 0x00, 0x0A, 0, 0 };
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    /* Flush old bytes from RX buffer */
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
    {
        volatile uint32_t dummy = huart2.Instance->RDR;
        (void)dummy;
    }
    __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);

    memset(pzem_rx_buf, 0, sizeof(pzem_rx_buf));

    /* Send 8-byte Modbus read request */
    if (HAL_UART_Transmit(&huart2, req, 8, 200) != HAL_OK)
    {
        sprintf(debug_buffer, "PZEM TX Failed\r\n");
        send_data_over_uart(debug_buffer);
        return PZEM_ERROR_TIMEOUT;
    }

    /* Receive 25-byte response with 600ms timeout (matching ESP32) */
    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&huart2, pzem_rx_buf, 25, 600);
    if (rx_status != HAL_OK)
    {
        sprintf(debug_buffer, "PZEM RX Timeout (status=%d)\r\n", rx_status);
        send_data_over_uart(debug_buffer);
        return PZEM_ERROR_TIMEOUT;
    }

    /* Verify Slave Address & Function Code */
    if (pzem_rx_buf[0] != PZEM_SLAVE_ADDR || pzem_rx_buf[1] != 0x04 || pzem_rx_buf[2] != 20)
    {
        sprintf(debug_buffer, "PZEM Header Error: %02X %02X %02X\r\n",
                pzem_rx_buf[0], pzem_rx_buf[1], pzem_rx_buf[2]);
        send_data_over_uart(debug_buffer);
        return PZEM_ERROR_CRC;
    }

    /* Verify CRC */
    if (!pzem_crc_valid(pzem_rx_buf, 25))
    {
        sprintf(debug_buffer, "PZEM CRC Error\r\n");
        send_data_over_uart(debug_buffer);
        return PZEM_ERROR_CRC;
    }

    /* Parse registers exactly matching working ESP32 code */
    uint16_t reg0 = pzem_reg(pzem_rx_buf, 0); /* Voltage (0.1V) */
    uint16_t reg1 = pzem_reg(pzem_rx_buf, 1); /* Current Low */
    uint16_t reg2 = pzem_reg(pzem_rx_buf, 2); /* Current High */
    uint16_t reg3 = pzem_reg(pzem_rx_buf, 3); /* Power Low */
    uint16_t reg4 = pzem_reg(pzem_rx_buf, 4); /* Power High */
    uint16_t reg5 = pzem_reg(pzem_rx_buf, 5); /* Energy Low */
    uint16_t reg6 = pzem_reg(pzem_rx_buf, 6); /* Energy High */
    uint16_t reg7 = pzem_reg(pzem_rx_buf, 7); /* Frequency (0.1Hz) */
    uint16_t reg8 = pzem_reg(pzem_rx_buf, 8); /* Power Factor (0.01) */

    data->voltage_dV    = reg0;                                        /* 0.1 V */
    data->current_mA    = (((uint32_t)reg2 << 16) | (uint32_t)reg1);    /* 1 mA  */
    data->power_dW      = (((uint32_t)reg4 << 16) | (uint32_t)reg3);    /* 0.1 W */
    data->energy_Wh     = (((uint32_t)reg6 << 16) | (uint32_t)reg5);    /* 1 Wh  */
    data->frequency_dHz = reg7;                                        /* 0.1 Hz*/
    data->power_factor  = reg8;                                        /* 0.01  */

    return PZEM_OK;
}

pzem_status_t PZEM_ReadOnce(pzem_data_t *data)
{
    /* USART2 is initialized at startup in main.c - just call Read directly */
    return PZEM_Read(data);
}
