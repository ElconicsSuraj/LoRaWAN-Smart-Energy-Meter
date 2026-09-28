/*
 * application_logic.c
 *
 *  Created on: Aug 21, 2025
 *      Author: Amogh MP
 */




#include "application_logic.h"
#include "main.h"
#include "usart.h"
#include "lora_app.h"
#include "utilities_def.h"
#include "protocol.h"
#include "utilities.h"
#include "pzem004t.h"
#include "stm32_systime.h"
#include "../LoRaWAN/config_application.h"



extern UTIL_TIMER_Object_t TxTimer;
extern UTIL_TIMER_Object_t SampleTimer;

extern uint32_t current_backoff_ms;

volatile uint8_t sw_pressed = 0;

char debug_buffer[256] = {'\0'};

uint8_t downlink_processing_buffer[256] = {0};
uint8_t downlink_data_buffer[256] = {0};

uint8_t power_up = 1;

/* Global variable for tracking join attempts */
uint32_t device_connection_retry_counter = 0;

GPIO_PinState sw_state = GPIO_PIN_SET;



device_status_e device_status = unknown;



void send_data_over_uart(char* str)
{
	HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 1000);
}



uint8_t get_battery_percentage(void)
{
	uint8_t battery_percentage = 0;

	uint32_t battery_value = ((uint64_t)device_config.battery_counter * 100) / NUMBER_OF_MESSAGES_PER_BATTERY_LIFE;

	if (battery_value >= 100)
	{
		battery_value = 99;
	}

	battery_percentage = 100 - battery_value;

	return battery_percentage;
}


void process_downlink_data(uint8_t* buffer, uint8_t payload_size)
{
	if ((0xB1 == buffer[0]) && (UPDATE_DEVICE_CONFIGURATION_DOWNLINK_SIZE == payload_size))
	{
		uint16_t sampling_interval = (buffer[1] << 8) | buffer[2];

		if ((sampling_interval >= DEFAULT_MINIMUM_SAMPLING_INTERVAL_IN_SEC) &&
				(sampling_interval < DEFAULT_MAXIMUM_SAMPLING_INTERVAL_IN_SEC))
		{
			device_config.sample_interval = sampling_interval;

			memset(buffer, 0, payload_size);

			erase_config_data();
			write_config_data(&device_config);
			HAL_Delay(500);
			device_config_data(&device_config);

			next_action.updated_configurations_received = 1;

			memset(debug_buffer, '\0', sizeof(debug_buffer));
			sprintf(debug_buffer, "Modified Sampling Interval Downlink Received: %d sec\r\n", device_config.sample_interval);
			send_data_over_uart(debug_buffer);
		}
	}

	else if((0xB2 == buffer[0]) && (FETCH_DEVICE_CONFIGURATION_DOWNLINK_SIZE == payload_size))
	{
		next_action.configuration_msg_received = 1;

		sprintf(debug_buffer, "%s\r\n", "Fetch Device Config Downlink Received");
		send_data_over_uart(debug_buffer);
	}

	else if ((0xB3 == buffer[0]) && (BATTERY_CALIBERATION_DOWNLINK_SIZE == payload_size))
	{
		next_action.battery_caliberation_request_received = 1;

		sprintf(debug_buffer, "%s\r\n", "Battery Calibration Downlink Received");
		send_data_over_uart(debug_buffer);
	}

	else if ((0xB4 == buffer[0]) && (ERASE_SAMPLES_DOWNLINK_SIZE == payload_size))
	{
		uint32_t saved_battery_counter = device_config.battery_counter;

		device_config.write_counter = 0;
		device_config.read_counter = 0;

		device_config.battery_counter = saved_battery_counter;

		erase_config_data();
		HAL_Delay(500);
		write_config_data(&device_config);
		HAL_Delay(500);
		flash_erase_one_page(SAMPLES_START_ADDR);

		write_sensor_data.sample_number = 0;

		next_action.erase_samples_request_received = 1;

		sprintf(debug_buffer, "Erase Samples Downlink Received. All pending samples cleared.\r\n");
		send_data_over_uart(debug_buffer);
	}
}

void check_if_user_switch_pressed(void)
{

  HAL_IWDG_Refresh(&hiwdg);

  if (1 == sw_pressed)
  {
	  sw_pressed = 0;

	  HAL_Delay(1000);
	  sw_state =  HAL_GPIO_ReadPin(USER_SW_GPIO_Port, USER_SW_Pin);

	  if (GPIO_PIN_RESET == sw_state)
	  {
		  HAL_Delay(1000);
		  sw_state =  HAL_GPIO_ReadPin(USER_SW_GPIO_Port, USER_SW_Pin);

		  HAL_Delay(1000);
		  sw_state =  HAL_GPIO_ReadPin(USER_SW_GPIO_Port, USER_SW_Pin);

		  if (GPIO_PIN_RESET == sw_state)
		  {
			  turn_on_led(BLUE);
			  HAL_Delay(5000);
			  turn_off_led(BLUE);

			  sprintf(debug_buffer, "Restoring the device to factory defaults\r\n");
			  send_data_over_uart(debug_buffer);

			  current_backoff_ms = 2000;

			  erase_config_data();
			  device_config.new_program = 0xFF;
			  write_config_data(&device_config);

			  //TODO: make sure sample counter/battery counter is retained

			  HAL_Delay(1000);


			  UTIL_SEQ_PauseTask(1 << CFG_SEQ_Task_LmHandlerProcess);
			  UTIL_SEQ_PauseTask(1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent);

			  HAL_Delay(1000);

			  HAL_NVIC_SystemReset();

		  }
	  }
   }
}


void run_application_logic(void)
{
	extern uint8_t can_transmit_data;
	HAL_IWDG_Refresh(&hiwdg);

	uint32_t pending = (device_config.write_counter > device_config.read_counter)
	                 ? (device_config.write_counter - device_config.read_counter) : 0;


#if RANGE_TESTING_MODE
	if (0) // Range testing: never pause sampling
#else
	if ((0 == can_transmit_data) && (pending >= FLASH_FULL_THRESHOLD))
#endif
	{
		sprintf(debug_buffer, "Sampling paused: link down & flash full (pending=%lu)\r\n",
		        (unsigned long)pending);
		send_data_over_uart(debug_buffer);
	}

	else
	{
		device_status = data_sampling;

		turn_on_led(GREEN);

		pzem_data_t pzem_data = {0};
		pzem_status_t pzem_res = PZEM_ReadOnce(&pzem_data);

		if (PZEM_OK == pzem_res)
		{
			write_sensor_data.voltage_dV    = pzem_data.voltage_dV;
			write_sensor_data.current_mA    = pzem_data.current_mA;
			write_sensor_data.power_dW      = pzem_data.power_dW;
			write_sensor_data.energy_Wh     = pzem_data.energy_Wh;
			write_sensor_data.frequency_dHz = pzem_data.frequency_dHz;
			write_sensor_data.power_factor  = pzem_data.power_factor;

			sprintf(debug_buffer, "PZEM: V=%u.%uV, I=%lu.%03luA, P=%lu.%uW, E=%luWh, F=%u.%uHz, PF=0.%02u\r\n",
					pzem_data.voltage_dV / 10, pzem_data.voltage_dV % 10,
					pzem_data.current_mA / 1000, pzem_data.current_mA % 1000,
					pzem_data.power_dW / 10, (unsigned int)(pzem_data.power_dW % 10),
					(unsigned long)pzem_data.energy_Wh,
					pzem_data.frequency_dHz / 10, pzem_data.frequency_dHz % 10,
					pzem_data.power_factor);
			send_data_over_uart(debug_buffer);
		}
		else
		{
			write_sensor_data.voltage_dV    = 0;
			write_sensor_data.current_mA    = 0;
			write_sensor_data.power_dW      = 0;
			write_sensor_data.energy_Wh     = 0;
			write_sensor_data.frequency_dHz = 0;
			write_sensor_data.power_factor  = 0;

			sprintf(debug_buffer, "PZEM Read Error: %d (transmitting status uplink)\r\n", pzem_res);
			send_data_over_uart(debug_buffer);
		}

		write_sensor_data.epoch_time    = SysTimeGet().Seconds;
		write_sensor_data.battery       = get_battery_percentage();

		uint8_t res = store_sample(&write_sensor_data);

		if (1 == res)
		{
			write_sensor_data.sample_number += 1;
			next_action.energy_meter_message_flag = 1;
		}

		turn_off_led(GREEN);
	}
}
