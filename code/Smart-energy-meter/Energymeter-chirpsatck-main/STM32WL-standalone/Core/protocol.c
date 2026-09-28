/*
 * protocol.c
 *
 *  Created on: Aug 21, 2025
 *      Author: Antz
 */




#include "main.h"
#include "protocol.h"
#include "temp_humidity_sensor.h"
#include "application_logic.h"
#include "eeprom.h"
#include "stm32_systime.h"


next_action_t next_action;



void create_periodic_message_packet(uint8_t *buf)
{
	memset(buf, 0, LORAWAN_APP_DATA_BUFFER_MAX_SIZE);
	memset(&read_sensor_data, 0, sizeof(read_sensor_data));

	read_next_sample(&read_sensor_data);

	buf[0] = periodic_message;

	buf[1] = (read_sensor_data.epoch_time & 0xFF000000) >> 24;
	buf[2] = (read_sensor_data.epoch_time & 0x00FF0000) >> 16;
	buf[3] = (read_sensor_data.epoch_time & 0x0000FF00) >> 8;
	buf[4] = read_sensor_data.epoch_time & 0xFF;

	buf[5] = (read_sensor_data.sample_number & 0xFF000000) >> 24;
	buf[6] = (read_sensor_data.sample_number & 0x00FF0000) >> 16;
	buf[7] = (read_sensor_data.sample_number & 0x0000FF00) >> 8;
	buf[8] = read_sensor_data.sample_number & 0xFF;


	buf[9] = (read_sensor_data.temperature & 0xFF00) >> 8;
	buf[10] = read_sensor_data.temperature & 0x00FF;
	buf[11] = read_sensor_data.humidity;

	buf[12] = get_battery_percentage();
}


void create_configuration_uplink_message(uint8_t *buf)
{
	memset(buf, 0, LORAWAN_APP_DATA_BUFFER_MAX_SIZE);

	buf[0] = configuration_retrieval_message;

	buf[1] = (device_config.battery_counter & 0xFF000000) >> 24;
	buf[2] = (device_config.battery_counter & 0x00FF0000) >> 16;
	buf[3] = (device_config.battery_counter & 0x0000FF00) >> 8;
	buf[4] = device_config.battery_counter & 0xFF;

	buf[5] = (device_config.sample_interval & 0xFF00) >> 8;
	buf[6] = device_config.sample_interval & 0x00FF;

	buf[7] = get_battery_percentage();
}


void create_battery_caliberation_ack_uplink_message(uint8_t *buf)
{
	memset(buf, 0, LORAWAN_APP_DATA_BUFFER_MAX_SIZE);

	buf[0] = battery_calibartion_response;
	buf[1] = get_battery_percentage();
}


void create_erase_samples_ack_uplink_message(uint8_t *buf)
{
	memset(buf, 0, LORAWAN_APP_DATA_BUFFER_MAX_SIZE);
	buf[0] = erase_samples_ack_response;
	buf[1] = get_battery_percentage();
}

void create_energy_meter_message_packet(uint8_t *buf)
{
	memset(buf, 0, LORAWAN_APP_DATA_BUFFER_MAX_SIZE);
	memset(&read_sensor_data, 0, sizeof(read_sensor_data));

	read_next_sample(&read_sensor_data);

	buf[0] = energy_meter_message; // 0xA6

	/* bytes 1..4: epoch time (4 bytes, big-endian) */
	buf[1] = (read_sensor_data.epoch_time & 0xFF000000) >> 24;
	buf[2] = (read_sensor_data.epoch_time & 0x00FF0000) >> 16;
	buf[3] = (read_sensor_data.epoch_time & 0x0000FF00) >> 8;
	buf[4] = read_sensor_data.epoch_time & 0xFF;

	/* bytes 5..6: voltage_dV (2 bytes, unit 0.1 V) */
	buf[5] = (read_sensor_data.voltage_dV & 0xFF00) >> 8;
	buf[6] = read_sensor_data.voltage_dV & 0x00FF;

	/* bytes 7..9: current_mA (3 bytes, unit 1 mA) */
	buf[7] = (read_sensor_data.current_mA & 0x00FF0000) >> 16;
	buf[8] = (read_sensor_data.current_mA & 0x0000FF00) >> 8;
	buf[9] = read_sensor_data.current_mA & 0x000000FF;

	/* bytes 10..12: power_dW (3 bytes, unit 0.1 W) */
	buf[10] = (read_sensor_data.power_dW & 0x00FF0000) >> 16;
	buf[11] = (read_sensor_data.power_dW & 0x0000FF00) >> 8;
	buf[12] = read_sensor_data.power_dW & 0x000000FF;

	/* bytes 13..16: energy_Wh (4 bytes, unit 1 Wh) */
	buf[13] = (read_sensor_data.energy_Wh & 0xFF000000) >> 24;
	buf[14] = (read_sensor_data.energy_Wh & 0x00FF0000) >> 16;
	buf[15] = (read_sensor_data.energy_Wh & 0x0000FF00) >> 8;
	buf[16] = read_sensor_data.energy_Wh & 0xFF;

	/* bytes 17..18: frequency_dHz (2 bytes, unit 0.1 Hz) */
	buf[17] = (read_sensor_data.frequency_dHz & 0xFF00) >> 8;
	buf[18] = read_sensor_data.frequency_dHz & 0x00FF;

	/* byte 19: power factor (1 byte, unit 0.01) */
	buf[19] = (uint8_t)(read_sensor_data.power_factor & 0xFF);

	/* byte 20: battery percentage */
	buf[20] = get_battery_percentage();
}

void create_vibration_message_packet(uint8_t *buf)
{
	memset(buf, 0, LORAWAN_APP_DATA_BUFFER_MAX_SIZE);

	uint32_t epoch_now = SysTimeGet().Seconds;

	buf[0] = vibration_message; // 0xA5

	/* bytes 1-4: live epoch time at the moment of this vibration event */
	buf[1] = (epoch_now & 0xFF000000) >> 24;
	buf[2] = (epoch_now & 0x00FF0000) >> 16;
	buf[3] = (epoch_now & 0x0000FF00) >> 8;
	buf[4] = epoch_now & 0xFF;

	/* byte 5: vibration flag (1 = vibration detected) */
	buf[5] = 0x01;

	/* byte 6: battery percentage */
	buf[6] = get_battery_percentage();
}
