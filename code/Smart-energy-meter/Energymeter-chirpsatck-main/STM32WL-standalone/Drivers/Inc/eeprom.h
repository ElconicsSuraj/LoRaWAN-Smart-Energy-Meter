/*
 * eeprom.h
 *
 *  Created on: Aug 21, 2025
 *      Author: Amogh MP
 */

#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_



#define DATA_EEPROM_BASE					0x0802D000 //TODO: Check this
#define DATA_EEPROM_END						0x0803D000 //TODO: Check this


#define FLASH_USER_START_ADDR   			DATA_EEPROM_BASE //(FLASH_BASE + FLASH_PAGE_SIZE * 96)       /* Start @ of user Flash area */
#define FLASH_USER_END_ADDR     			DATA_EEPROM_END  //FLASH_USER_START_ADDR + FLASH_PAGE_SIZE * 32   /* End @ of user Flash area */



#define SAMPLES_START_ADDR					0x08030000 // 10 pages: 0x08030000 to 0x08035000
#define PAGE_SIZE 							2048U
#define NUM_SAMPLE_PAGES 					10U
#define SAMPLES_PER_PAGE 					(PAGE_SIZE / sizeof(sample_data_t)) // 64
#define MAX_SAMPLES 						(NUM_SAMPLE_PAGES * SAMPLES_PER_PAGE) // 640



#define START_ADDRESS_OF_DEVNONCE_COUNTER	0x0803E000

#define DATA_32                 ((uint32_t) 2)


typedef struct config
{
	uint32_t battery_counter;
	uint32_t write_counter;
	uint32_t read_counter;
	uint16_t new_program;
	uint16_t sample_interval; // in sec
	uint16_t upload_interval; // in sec
	uint8_t is_reset_battery_counter;

	uint8_t dummy[13];

}device_config_t;

extern device_config_t device_config;



typedef struct sample_data_s
{
	uint32_t epoch_time;
	uint32_t sample_number;

	uint16_t voltage_dV;     /* 0.1 V resolution (e.g. 2300 = 230.0 V) */
	uint16_t frequency_dHz;  /* 0.1 Hz resolution (e.g. 500 = 50.0 Hz) */
	uint32_t current_mA;     /* 1 mA resolution (e.g. 5000 = 5.000 A) */
	uint32_t power_dW;       /* 0.1 W resolution (e.g. 11500 = 1150.0 W) */
	uint32_t energy_Wh;      /* 1 Wh resolution (e.g. 1234 Wh) */
	uint16_t power_factor;   /* 0.01 resolution (e.g. 98 = 0.98) */
	uint16_t temperature;    /* Legacy / optional temp reading */
	uint8_t  humidity;       /* Legacy / optional humidity reading */
	uint8_t  battery;
	uint8_t  reserved[2];    /* Padded to 32 bytes for flash double-word alignment */
}sample_data_t;

extern sample_data_t write_sensor_data, read_sensor_data;


int write_config_data(device_config_t *configuration);
void read_config_data(device_config_t *configuration);
int erase_config_data(void);
uint8_t flash_erase_one_page(uint32_t address);


void init_sample_storage(void);
uint8_t store_sample(const sample_data_t *sample);
uint8_t read_next_sample(sample_data_t *sample);



#endif /* INC_EEPROM_H_ */
