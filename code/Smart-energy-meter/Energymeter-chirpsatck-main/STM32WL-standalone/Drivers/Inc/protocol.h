/*
 * protocol.h
 *
 *  Created on: Aug 21, 2025
 *      Author: Amogh MP
 */

#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_


#include <stdint.h>
#include "LmHandler.h"
#include "lora_app.h"


#define NUMBER_OF_MESSAGES_PER_BATTERY_LIFE			72000



#define PERIODIC_MESSAGE_SIZE						13
#define ENERGY_METER_MESSAGE_SIZE					21
#define VIBRATION_MESSAGE_SIZE						7  // msg id (1) + epoch time (4) + vibration flag (1) + battery (1)
#define DEVICE_CONFIGURATION_ACK_MESSAGE_SIZE		8
#define BATTERY_CALIBRATION_ACK_MESSAGE_SIZE		2
#define ERASE_SAMPLES_ACK_MESSAGE_SIZE				2



#define UPDATE_DEVICE_CONFIGURATION_DOWNLINK_SIZE	3
#define FETCH_DEVICE_CONFIGURATION_DOWNLINK_SIZE	1
#define BATTERY_CALIBERATION_DOWNLINK_SIZE			2
#define ERASE_SAMPLES_DOWNLINK_SIZE					1




typedef enum
{
	periodic_message = 0xA1,
	configuration_retrieval_message,
	battery_calibartion_response,
	erase_samples_ack_response,    // 0xA4
	vibration_message,             // 0xA5
	energy_meter_message           // 0xA6
}uplink_message_types;


typedef struct
{
	uint8_t periodic_message_flag;
	uint8_t energy_meter_message_flag;
	uint8_t updated_configurations_received;
	uint8_t configuration_msg_received;
	uint8_t battery_caliberation_request_received;
	uint8_t erase_samples_request_received;
	uint8_t vibration_detected;
}next_action_t;

extern next_action_t next_action;

void create_periodic_message_packet(uint8_t *buf);
void create_energy_meter_message_packet(uint8_t *buf);
void create_configuration_uplink_message(uint8_t *buf);
void create_battery_caliberation_ack_uplink_message(uint8_t *buf);
void create_erase_samples_ack_uplink_message(uint8_t *buf);
void create_vibration_message_packet(uint8_t *buf);



#endif /* INC_PROTOCOL_H_ */
