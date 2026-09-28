/*
 * temp_humidity_sensor.c
 *
 *  Created on: Aug 17, 2025
 *      Author: Amogh MP
 */





#include "temp_humidity_sensor.h"
#include "i2c.h"
#include "eeprom.h"

 char buffer[40];


char flash_storage_buffer[32] = {'\0'};


uint8_t hum_tx_data[10] = {0};
uint8_t hum_rx_data[10] = {0};

const uint8_t SHT40_CMD_MEASURE_HIGH[] = {0xFD};


HAL_StatusTypeDef valid_temperature = HAL_ERROR;



void delay_us(uint32_t time)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < time);
}


void set_pin_output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void set_pin_input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}



uint8_t ds18b20_start(void)
{
    uint8_t response = 0;

    set_pin_output(DS18B20_PORT, DS18B20_PIN);

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    delay_us(480);

    set_pin_input(DS18B20_PORT, DS18B20_PIN);

    delay_us(80);

    if (!HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN))
    {
    	response = 1;
    }
    else
	{
		response = 0;
	}

    delay_us(400);

    return response;
}


uint8_t ds18b20_read(void)
{
    uint8_t value = 0;
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    for (uint8_t i = 0 ; i < 8 ; i++)
    {
    	set_pin_output(DS18B20_PORT, DS18B20_PIN);

        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);

        delay_us(2);

        set_pin_input(DS18B20_PORT, DS18B20_PIN);

        if (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN))
        {
            value |= (1 << i);
        }

        delay_us(60);
    }

    return value;
}


void ds18b20_write(uint8_t data)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    for (uint8_t i = 0 ; i < 8 ; i++)
    {
    	set_pin_output(DS18B20_PORT, DS18B20_PIN);

        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);

        delay_us(1);

        if (data & (1 << i))
        {
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
            delay_us(60);
        }
        else
        {
            delay_us(60);
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
        }
    }
}


uint8_t sensirion_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;

    for (uint16_t i = 0 ; i < len ; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0 ; bit < 8 ; bit++)
        {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}




void read_humidity_and_temperature(int16_t *temp, uint8_t *hum)
{
	float temperature;
	float humidity;


#if USE_ONBOARD_SHTC3_SENSOR

	uint16_t temp_value = 0;
	int counter = 0;

	MX_I2C1_Init();
	HAL_Delay(100);

	hum_tx_data[0] = 0x35;
	hum_tx_data[1] = 0x17;

	valid_temperature = HAL_I2C_Master_Transmit(&hi2c1, ONBOARD_SHTC3_SENSOR_CHIP_ADDRESS, hum_tx_data, 2, 1000);
	HAL_Delay(100);

	if (HAL_OK == valid_temperature)
	{
	  hum_tx_data[0] = 0x64;
	  hum_tx_data[1] = 0x58;

	  valid_temperature = HAL_I2C_Master_Transmit(&hi2c1, ONBOARD_SHTC3_SENSOR_CHIP_ADDRESS, hum_tx_data, 2, 1000);
	  HAL_Delay(100);

	  if (HAL_OK == valid_temperature)
	  {
		  valid_temperature = HAL_I2C_Master_Receive(&hi2c1, ONBOARD_SHTC3_SENSOR_CHIP_ADDRESS, hum_rx_data, 6, 2000);

		  if (HAL_OK == valid_temperature)
		  {
			  temp_value = hum_rx_data[1] | (hum_rx_data[0] << 8);
			  temperature = ((float)(175 * temp_value) / (float)65536) - (float)45.00;
			  temperature *= 10.0;
			  *temp =  (int16_t)floor(temperature);

			  temp_value = hum_rx_data[4] | (hum_rx_data[3] << 8);
			  humidity = ((float)(100 * temp_value) / 65535);
			  *hum =  (uint16_t)floor(humidity);
		  }
	  	}
	}

    if (HAL_OK != valid_temperature)
    {
    	*temp = 1000;
    	*hum = 0;
    }

	hum_tx_data[0] = 0xB0;
	hum_tx_data[1] = 0x98;

	HAL_I2C_Master_Transmit(&hi2c1, ONBOARD_SHTC3_SENSOR_CHIP_ADDRESS, hum_tx_data, 2, 1000);
	HAL_Delay(100);

	HAL_I2C_DeInit(&hi2c1);
	HAL_Delay(100);


#endif

#if USE_DS18B20_PROBE_SENSOR

    uint8_t temp_l = 0;
    uint8_t temp_h = 0;

    MX_TIM2_Init();
	HAL_TIM_Base_Start(&htim2);

    ds18b20_start();
    ds18b20_write(0xCC);  // Skip ROM
    ds18b20_write(0x44);  // Convert T
    HAL_Delay(750);

    ds18b20_start();
    ds18b20_write(0xCC);  // Skip ROM
    ds18b20_write(0xBE);  // Read Scratchpad

    temp_l = ds18b20_read();
    temp_h = ds18b20_read();

    temperature = (float)((temp_h << 8) | temp_l) / 16.0;

    temperature *= 10.0;
    *temp =  (int16_t)floor(temperature);

    *hum = 0;

#endif


#if USE_SHT40_PROBE_SENSOR

    MX_I2C1_Init();
    uint8_t rx[6];

    valid_temperature = HAL_I2C_Master_Transmit(&hi2c1, SHT40_ADDR_WRITE, (uint8_t *)SHT40_CMD_MEASURE_HIGH, 1, 100);
    HAL_Delay(SHT40_PROBE_SENSOR_MEASUREMENT_DELAY_MS);

	  if (HAL_OK == valid_temperature)
	  {
		  valid_temperature = HAL_I2C_Master_Receive(&hi2c1, SHT40_ADDR_READ, rx, 6, 100);

		  if (HAL_OK == valid_temperature)
		  {
			    uint16_t rawT = (rx[0] << 8) | rx[1];
			    uint8_t crcT  = rx[2];
			    uint16_t rawRH = (rx[3] << 8) | rx[4];
			    uint8_t crcRH  = rx[5];

			    if (sensirion_crc8(rx + 0, 2) == crcT)
			    {
					if (sensirion_crc8(rx + 3, 2) == crcRH)
					{
					    *temp = -45.0f + 175.0f * ( (float)rawT / 65535.0f);
					    *hum   = -6.0f  + 125.0f * ( (float)rawRH / 65535.0f);
					    *temp = 10*(*temp);
					}
					else
					{
				    	*temp = 1000;
				    	*hum = 0;
					}
			    }
			    else
			    {
			    	*temp = 1000;
			    	*hum = 0;
			    }
		  }

	  }

	  if (HAL_OK != valid_temperature)
	  {
	    	*temp = 1000;
	    	*hum = 0;
	  }


#endif

	sprintf(buffer, "Temp: %d  ", *temp);
	send_data_over_uart(buffer);

	sprintf(buffer, "Hum: %d\r\n", *hum);
	send_data_over_uart(buffer);
}
