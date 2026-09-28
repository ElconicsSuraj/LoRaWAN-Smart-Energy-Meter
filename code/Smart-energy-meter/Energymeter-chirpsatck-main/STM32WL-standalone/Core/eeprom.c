/*
 * eeprom.c
 *
 *  Created on: Aug 21, 2025
 *      Author: Amogh MP
 */




#include "main.h"
#include "eeprom.h"
#include "application_logic.h"


uint32_t page_error = 0;

FLASH_EraseInitTypeDef FLash_Write_Counter_Struct;


device_config_t device_config;
sample_data_t write_sensor_data, read_sensor_data;



static struct
{
    uint32_t current_write_addr;
} storage_state;


static int is_erased(uint32_t addr)
{
    for (uint32_t i = 0; i < sizeof(sample_data_t); i += 8)
    {
        if (*(__IO uint64_t *)(addr + i) != 0xFFFFFFFFFFFFFFFFULL)
        {
            return 0;
        }
    }
    return 1;
}



int write_config_data(device_config_t *configuration)
{
	int i = 0;
	uint64_t temp_data[4];
	uint32_t address;

	HAL_StatusTypeDef result = HAL_ERROR;

	memcpy(&temp_data[0], configuration, sizeof(device_config_t));

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

	address = DATA_EEPROM_BASE;

	for (i = 0 ; i < 4 ; i++)
	{
		result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, temp_data[i]);
		if (result != HAL_OK)
		{
			break;
		}
		address = address + 8;
	}

	HAL_FLASH_Lock();

	return result;
}


void device_config_data(device_config_t *configuration)
{
	int i = 0;
	uint64_t temp_data[4] = {0};
	uint8_t buffer[32] = {0}; // make sure buffer size = sizeof(device_config_t)
	uint32_t address;

	address = DATA_EEPROM_BASE;

	for(i = 0 ; i < 4 ; i++)
	{
		temp_data[i] = *(__IO uint64_t *)address;
		address = address + 8;
	}

	memcpy(buffer, &temp_data[0], sizeof(device_config_t));
	memcpy(configuration, &buffer[0], sizeof(device_config_t));
}


int erase_config_data(void)
{
	FLash_Write_Counter_Struct.TypeErase   = FLASH_TYPEERASE_PAGES;
//	FLash_Write_Counter_Struct.Page        = 90;
	FLash_Write_Counter_Struct.Page        = (DATA_EEPROM_BASE - FLASH_BASE) / PAGE_SIZE;
	FLash_Write_Counter_Struct.NbPages     = 1;

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

	if (HAL_FLASHEx_Erase(&FLash_Write_Counter_Struct, &page_error) != HAL_OK)
	{
		HAL_FLASH_Lock();
		return 0;
	}

	HAL_FLASH_Lock();

	return 1;
}


uint32_t GetPage(uint32_t Addr)
{
  return (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;;
}


uint8_t flash_erase_one_page(uint32_t address)
{
	uint32_t FirstPage = GetPage(address);

	FLash_Write_Counter_Struct.TypeErase   = FLASH_TYPEERASE_PAGES;
	FLash_Write_Counter_Struct.Page        = FirstPage;
	FLash_Write_Counter_Struct.NbPages     = 1;

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

	if (HAL_FLASHEx_Erase(&FLash_Write_Counter_Struct, &page_error) != HAL_OK)
	{
		HAL_FLASH_Lock();
		return 0;
	}

	HAL_FLASH_Lock();

	return 1;
}


uint8_t write_data(uint32_t address, uint64_t data)
{
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

	if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data) != HAL_OK)
	{
		HAL_FLASH_Lock();
		return 0;
	}

	HAL_FLASH_Lock();

	return 1;
}

uint64_t get_devnonce_counter(void)
{

}

uint64_t update_devnonce_counter(uint64_t counter)
{

}



static void read_sample_data(uint32_t addr, sample_data_t *sample)
{
    uint64_t data[4];

    for (int i = 0; i < 4; i++)
    {
        data[i] = *(__IO uint64_t *)(addr + i * 8);
    }
    memcpy(sample, data, sizeof(sample_data_t));
}


void init_sample_storage(void)
{
    device_config_data(&device_config);

    if (0xAA != device_config.new_program)
    {
		device_config.write_counter = 0;
		device_config.read_counter = 0;

		device_config.upload_interval = DEFAULT_UPLOAD_INTERVAL_IN_SECONDS;
		device_config.sample_interval = DEFAULT_MINIMUM_SAMPLING_INTERVAL_IN_SEC;

		device_config.new_program = 0xAA;

		if (0 != device_config.is_reset_battery_counter)
		{
			device_config.battery_counter = 0;
			device_config.is_reset_battery_counter = 0;
		}

        erase_config_data();
        write_config_data(&device_config);
    }

    if (device_config.write_counter == 0)
    {
        flash_erase_one_page(SAMPLES_START_ADDR);
    }

    if (device_config.write_counter > 0)
    {
    	uint32_t last_addr = SAMPLES_START_ADDR + ((device_config.write_counter - 1) % MAX_SAMPLES) * sizeof(sample_data_t);
    	read_sample_data(last_addr, &write_sensor_data);

    	write_sensor_data.sample_number += 1;


    	if (device_config.read_counter > 0)
    	{
    		last_addr = SAMPLES_START_ADDR + ((device_config.read_counter - 1) % MAX_SAMPLES) * sizeof(sample_data_t);
    		read_sample_data(last_addr, &read_sensor_data);
    	}
    }

    if (device_config.read_counter > device_config.write_counter)
    {
        device_config.read_counter = device_config.write_counter;

        erase_config_data();
        write_config_data(&device_config);
    }

    uint32_t sample_idx = device_config.write_counter % MAX_SAMPLES;
    storage_state.current_write_addr = SAMPLES_START_ADDR + sample_idx * sizeof(sample_data_t);

    if (device_config.write_counter > 0 && sample_idx % SAMPLES_PER_PAGE == 0)
    {
        flash_erase_one_page(storage_state.current_write_addr);
    }
}


uint8_t store_sample(const sample_data_t *sample)
{
	if (NULL == sample)
	{
		return 0;
	}

    device_config_data(&device_config);


	uint32_t pending = (device_config.write_counter > device_config.read_counter)
			? device_config.write_counter - device_config.read_counter : 0;

	if (pending >= MAX_SAMPLES)
	{
		device_config.read_counter = device_config.write_counter - MAX_SAMPLES + 1;
	}

    uint32_t sample_idx = device_config.write_counter % MAX_SAMPLES;
    uint32_t addr = SAMPLES_START_ADDR + sample_idx * sizeof(sample_data_t);

    if ((device_config.write_counter > 0) && (0 == sample_idx))
    {
    	if (1 != flash_erase_one_page(SAMPLES_START_ADDR))
    	{
    		sprintf(debug_buffer, "erase failed addr: 0x%08lX\r\n", SAMPLES_START_ADDR);
			send_data_over_uart(debug_buffer);

			return 0;
		}

		sprintf(debug_buffer, "erase success addr: 0x%08lX\r\n", SAMPLES_START_ADDR);
		send_data_over_uart(debug_buffer);

        uint32_t read_idx = device_config.read_counter % MAX_SAMPLES;

        if ((pending > 0) && (read_idx < SAMPLES_PER_PAGE))
        {
            device_config.read_counter += SAMPLES_PER_PAGE - read_idx;
        }
        addr = SAMPLES_START_ADDR;
    }

    else if ((device_config.write_counter > 0) && (0 == (sample_idx % SAMPLES_PER_PAGE)))
    {
    	if (flash_erase_one_page(addr) != 1)
    	{
			sprintf(debug_buffer, "erase failed addr: 0x%08lX\r\n", addr);
			send_data_over_uart(debug_buffer);

			return 0;
		}

		sprintf(debug_buffer, "erase success addr: 0x%08lX\r\n", addr);
		send_data_over_uart(debug_buffer);

        uint32_t page_idx = sample_idx / SAMPLES_PER_PAGE;
        uint32_t read_page_idx = device_config.read_counter % MAX_SAMPLES / SAMPLES_PER_PAGE;

        if ((pending > 0) && (read_page_idx == page_idx))
        {
            device_config.read_counter += SAMPLES_PER_PAGE - (device_config.read_counter % SAMPLES_PER_PAGE);
        }
    }

    uint64_t data[4];
    memcpy(data, sample, sizeof(sample_data_t));

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

    uint32_t timeout = 1000;

    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) && timeout--)
    {
    	HAL_Delay(1);
    }

    if (timeout == 0)
    {
		snprintf(debug_buffer, sizeof(debug_buffer), "Flash busy timeout\r\n");
		send_data_over_uart(debug_buffer);

		HAL_FLASH_Lock();

		return 0;
	}

    for (int i = 0; i < 4; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i * 8, data[i]) != HAL_OK)
        {
        	sprintf(debug_buffer, "Flash write failed at addr 0x%08lX, error=0x%08lX\r\n", addr + i * 8, HAL_FLASH_GetError());
			send_data_over_uart(debug_buffer);

            HAL_FLASH_Lock();

            return 0;
        }

        HAL_Delay(1);
    }

    HAL_FLASH_Lock();

    device_config.write_counter += 1;
    storage_state.current_write_addr = addr + sizeof(sample_data_t);

    erase_config_data();

    if (write_config_data(&device_config) != HAL_OK)
    {
		sprintf(debug_buffer, "write failed\r\n");
		send_data_over_uart(debug_buffer);

		return 0;
	}

    sprintf(debug_buffer, "sample: %d - 0x%08lX\r\n", device_config.write_counter - 1, addr);
    send_data_over_uart(debug_buffer);

    return 1;
}


uint8_t read_next_sample(sample_data_t *sample)
{
    device_config_data(&device_config);

    if (device_config.read_counter >= device_config.write_counter)
    {
        return 0;
    }

    uint32_t sample_idx = device_config.read_counter % MAX_SAMPLES;
    uint32_t addr = SAMPLES_START_ADDR + sample_idx * sizeof(sample_data_t);


    if (is_erased(addr))
    {
        return 0;
    }

    read_sample_data(addr, sample);

    return 1;
}
