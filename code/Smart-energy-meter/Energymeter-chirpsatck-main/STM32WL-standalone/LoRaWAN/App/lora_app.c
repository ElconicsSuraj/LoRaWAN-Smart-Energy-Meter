
#include "platform.h"
#include "Region.h" /* Needed for LORAWAN_DEFAULT_DATA_RATE */
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "lora_app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "stm32_lpm.h"
#include "adc_if.h"
#include "sys_conf.h"
#include "radio.h"
#include "send_raw_lora.h"
#include "stdlib.h"

#define LINE_SIZE 34
#include  "General_Setup.h"



#include "gpio.h"
#include "application_logic.h"
#include "eeprom.h"
#include "flash_if.h"
#include "protocol.h"



extern uint8_t power_up;



static void byteReception(uint8_t *PData, uint16_t Size, uint8_t Error);
#define RX_BUFF_SIZE 250


static uint8_t rxBuff[RX_BUFF_SIZE];
uint8_t isRxConfirmed;
uint32_t LoRaMode = 0;
uint8_t size_txBUFFER = 0;
uint8_t txBUFFER[100];
uint8_t isTriggered = 0;



static uint8_t rx_byte;
static uint32_t rx_counter = 0;
uint8_t downlink_data_length = 0;

uint8_t can_transmit_data = 0;

static uint32_t transmission_failure_counter = 0;


uint32_t current_backoff_ms = 2000;
static uint32_t max_backoff_ms = 3600000; /* Max backoff: 1 hour */


uint8_t link_check_test_buffer[1] = {0};
LmHandlerAppData_t link_check_payload = {.Buffer = link_check_test_buffer, .BufferSize = 0, .Port = 1};
UTIL_TIMER_Time_t nextTxIn = 0;



typedef enum TxEventType_e
{
	TX_ON_TIMER,
	TX_ON_EVENT
} TxEventType_t;

static void SendTxData(void);
static void OnTxTimerEvent(void *context);
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);
static void OnTxData(LmHandlerTxParams_t *params);
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);
static void OnMacProcessNotify(void);

static void OnTxTimerLedEvent(void *context);
static void OnRxTimerLedEvent(void *context);
static void OnJoinTimerLedEvent(void *context);



static void OnJoinBackoffTimerEvent(void *context);

static void OnSampleTimerEvent(void *context);


static void OnMlmeConfirm( MlmeConfirm_t *mlmeConfirm ); // TODO: AMP 21-08-2025

static void OnClockSyncTimerEvent(void *context);

static void OnLinkCheckTimerEvent(void *context);
static void OnIWDGRefreshTimerEvent(void *context);


static void OnSysTimeUpdate(void *context);


static void MX_BP_IT_Init(void);

static void while_loop(void);

static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

static LmHandlerCallbacks_t LmHandlerCallbacks =
{
		.GetBatteryLevel =           GetBatteryLevel,
		.GetTemperature =            GetTemperatureLevel,
		.GetUniqueId =               GetUniqueId,
		.GetDevAddr =                GetDevAddr,
		.OnMacProcess =              OnMacProcessNotify,
		.OnJoinRequest =             OnJoinRequest,
		.OnTxData =                  OnTxData,
		.OnRxData =                  OnRxData,

		.OnMlmeConfirm = 			OnMlmeConfirm, // TODO: AMP 21-08-2025
		.OnSysTimeUpdate = 			OnSysTimeUpdate, // TODO: AMP 29-08-2025
};

static LmHandlerParams_t LmHandlerParams =
{
		.ActiveRegion =             ACTIVE_REGION,
		.DefaultClass =             LORAWAN_DEFAULT_CLASS,
		.AdrEnable =                LORAWAN_ADR_STATE,
		.TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
		.PingPeriodicity =          LORAWAN_DEFAULT_PING_SLOT_PERIODICITY
};

//static TxEventType_t EventType = ADMIN_TX_TYPE;
UTIL_TIMER_Object_t TxTimer;
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };
static uint8_t AppLedStateOn = RESET;
static UTIL_TIMER_Object_t TxLedTimer;
static UTIL_TIMER_Object_t RxLedTimer;
static UTIL_TIMER_Object_t JoinLedTimer;


static UTIL_TIMER_Object_t JoinBackoffTimer;
UTIL_TIMER_Object_t SampleTimer;
static UTIL_TIMER_Object_t ClockSyncTimer;
static UTIL_TIMER_Object_t LinkCheckTimer;
static UTIL_TIMER_Object_t IWDGRefreshTimer;


void print_keys_and_id_onto_console(void)
{
	Key_t *keyItem;

	uint8_t *devEui = SecureElementGetDevEui();
	sprintf(debug_buffer, "DeviceEUI -> %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			devEui[0], devEui[1], devEui[2], devEui[3], devEui[4], devEui[5], devEui[6], devEui[7]);
	send_data_over_uart(debug_buffer);

	uint8_t *joinEui = SecureElementGetJoinEui();
	sprintf(debug_buffer, "JoinEUI -> %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			joinEui[0], joinEui[1], joinEui[2], joinEui[3], joinEui[4], joinEui[5], joinEui[6], joinEui[7]);
	send_data_over_uart(debug_buffer);

	SecureElementGetKeyByID(NWK_KEY, &keyItem);
	sprintf(debug_buffer, "NwkKey -> %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			keyItem->KeyValue[0], keyItem->KeyValue[1], keyItem->KeyValue[2], keyItem->KeyValue[3], keyItem->KeyValue[4],
			keyItem->KeyValue[5], keyItem->KeyValue[6], keyItem->KeyValue[7], keyItem->KeyValue[8], keyItem->KeyValue[9],
			keyItem->KeyValue[10], keyItem->KeyValue[11], keyItem->KeyValue[12], keyItem->KeyValue[13], keyItem->KeyValue[14],
			keyItem->KeyValue[15]);
	send_data_over_uart(debug_buffer);

	SecureElementGetKeyByID(NWK_KEY, &keyItem);
	sprintf(debug_buffer, "AppKey -> %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			keyItem->KeyValue[0], keyItem->KeyValue[1], keyItem->KeyValue[2], keyItem->KeyValue[3], keyItem->KeyValue[4],
			keyItem->KeyValue[5], keyItem->KeyValue[6], keyItem->KeyValue[7], keyItem->KeyValue[8], keyItem->KeyValue[9],
			keyItem->KeyValue[10], keyItem->KeyValue[11], keyItem->KeyValue[12], keyItem->KeyValue[13], keyItem->KeyValue[14],
			keyItem->KeyValue[15]);
	send_data_over_uart(debug_buffer);
}



void LoRaWAN_Init(void)
{
	device_status = joining;

	UTIL_TIMER_Create(&TxLedTimer, 500, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
	UTIL_TIMER_Create(&RxLedTimer, 500, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
	UTIL_TIMER_Create(&JoinLedTimer, 500, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);


	if (FLASH_IF_Init(NULL) != FLASH_IF_OK)
	{
		Error_Handler();
	}


	UTIL_TIMER_Create(&SampleTimer, (device_config.sample_interval * 1000), UTIL_TIMER_PERIODIC, OnSampleTimerEvent, NULL);
	UTIL_TIMER_Create(&TxTimer, (device_config.upload_interval * 1000), UTIL_TIMER_PERIODIC, OnTxTimerEvent, NULL);
	UTIL_TIMER_Create(&JoinBackoffTimer, current_backoff_ms, UTIL_TIMER_ONESHOT, OnJoinBackoffTimerEvent, NULL);

	UTIL_TIMER_Create(&LinkCheckTimer, LINK_CHECK_RETRY_INTERVAL_MS, UTIL_TIMER_PERIODIC, OnLinkCheckTimerEvent, NULL);
	UTIL_TIMER_Create(&ClockSyncTimer, 10 * 1000, UTIL_TIMER_PERIODIC, OnClockSyncTimerEvent, NULL);

	UTIL_TIMER_Create(&IWDGRefreshTimer, 25 * 1000, UTIL_TIMER_PERIODIC, OnIWDGRefreshTimerEvent, NULL);

	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LmHandlerProcess), UTIL_SEQ_RFU, LmHandlerProcess);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), UTIL_SEQ_RFU, SendTxData);


	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_RunApplicationLogic), UTIL_SEQ_RFU, run_application_logic);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_CheckUserSwitchStatus), UTIL_SEQ_RFU, check_if_user_switch_pressed);


	LoraInfo_Init();

	/* Initialize all callbacks */
	LmHandlerInit(&LmHandlerCallbacks);

	/* Print LoRaWAN information : DevEUI & Devaddr when ABP - DevEUI & AppEUI-JoinEUI for OTAA */
	/* Print Session Keys for ABP - Print Root Key for OTAA :LmHandlerConfigure() > LoRaMacInitialization() > SecureElementInit() > PrintKey() */
	LmHandlerConfigure(&LmHandlerParams);

	print_keys_and_id_onto_console();

	/* Let all print out terminated. Otherwise logs are affected.*/
	HAL_Delay(500);

	/* Join Red LED starts blinking */
	UTIL_TIMER_Start(&JoinLedTimer);

	/* Join procedure for OTAA */
	/* First try to Join Network. Next time the Device tries to send data (LmHandlerSend), it will check the Join.
	 * If the first Join was NOT successful, it sends another Join.
	 */
	LmHandlerJoin(ActivationType);
}




//static void byteReception(uint8_t *PData, uint16_t Size, uint8_t Error)
//{
//	rx_byte = *PData;
//
//	if (rx_counter >= (RX_BUFF_SIZE - 4))
//	{
//		rx_counter = 0;
//	}
//	else if (rx_byte == '\n')
//	{
//		downlink_data_buffer[rx_counter++] = '\n';
//
//		if (rx_counter >= 12)
//		{
//			memset(downlink_processing_buffer, 0, sizeof(downlink_processing_buffer));
//			memcpy(downlink_processing_buffer, downlink_data_buffer, rx_counter);
//
//
//			rx_counter = 0;
//
//			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
//
//			APP_LOG(0, 1, "\r\n");
//
//		}
//		else
//		{
//			rx_counter = 0;
//		}
//	}
//	else
//	{
//		downlink_data_buffer[rx_counter++] = rx_byte;
//		APP_LOG(0, 1, "%02X", rx_byte);
//	}
//}




static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
	HAL_IWDG_Refresh(&hiwdg);

	turn_off_led(GREEN);
	turn_off_led(BLUE);
	turn_off_led(RED);

	if (joinParams != NULL)
	{
		if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
		{
			UTIL_TIMER_Stop(&JoinLedTimer);
			UTIL_TIMER_Stop(&JoinBackoffTimer);

			device_status = join_successful;

			device_connection_retry_counter = 0;
			current_backoff_ms = 2000;

			if (joinParams->Mode == ACTIVATION_TYPE_ABP)
			{
				sprintf(debug_buffer, "ABP ======================\r\n");
				send_data_over_uart(debug_buffer);
			}
			else
			{
				sprintf(debug_buffer, "OTAA ======================\r\n");
				send_data_over_uart(debug_buffer);
			}

			HAL_Delay(500);

			can_transmit_data = 1;
			power_up = 0;

			UTIL_TIMER_Start(&IWDGRefreshTimer);

			/* Start periodic PZEM sampling and transmission timers */
			UTIL_TIMER_Stop(&SampleTimer);
			UTIL_TIMER_Start(&SampleTimer);

			UTIL_TIMER_Stop(&TxTimer);
			UTIL_TIMER_Start(&TxTimer);

			/* Trigger initial reading & transmission right after join */
			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_RunApplicationLogic), CFG_SEQ_Prio_0);
		}
		else
		{
			device_status = join_fail;

			sprintf(debug_buffer, "JOIN FAILED: %d\r\n", joinParams->Status);
			send_data_over_uart(debug_buffer);

			device_connection_retry_counter++;

			if (device_connection_retry_counter >= LORAWAN_MAX_RETRY_JOIN_ATTEMPTS)
			{
				device_connection_retry_counter = 0;
				current_backoff_ms = 2000; /* Reset backoff to initial value */

				sprintf(debug_buffer, "Max join attempts reached. Reset Now\r\n", LORAWAN_MAX_RETRY_JOIN_ATTEMPTS);
				send_data_over_uart(debug_buffer);

				HAL_Delay(100);  /* Allow time for message transmission */

				UTIL_SEQ_PauseTask((1 << CFG_SEQ_Task_LmHandlerProcess));
				UTIL_SEQ_PauseTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent));


				NVIC_SystemReset();
			}

			/* Start exponential backoff timer */
			sprintf(debug_buffer, "Next join attempt in %lu ms\r\n", current_backoff_ms);
			send_data_over_uart(debug_buffer);

			turn_off_led(GREEN);
			turn_off_led(BLUE);
			turn_off_led(RED);

			UTIL_TIMER_SetPeriod(&JoinBackoffTimer, current_backoff_ms);
			UTIL_TIMER_Start(&JoinBackoffTimer);

			/* Double the backoff time for next attempt, but don't exceed maximum */
			current_backoff_ms = (current_backoff_ms * 2 <= max_backoff_ms) ?
					current_backoff_ms * 2 : max_backoff_ms;
		}
	}
}


static void OnJoinBackoffTimerEvent(void *context)
{
	HAL_IWDG_Refresh(&hiwdg);

	/* Backoff period is over, try joining again */
	sprintf(debug_buffer, "Backoff period ended, attempting to join\r\n");
	send_data_over_uart(debug_buffer);

	turn_on_led(BLUE);

	/* Trigger a new join attempt */
	LmHandlerJoin(ActivationType);
}



static void OnMacProcessNotify(void)
{
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LmHandlerProcess), CFG_SEQ_Prio_0);
}


static void OnSampleTimerEvent(void *context)
{
	HAL_IWDG_Refresh(&hiwdg);

	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_RunApplicationLogic), CFG_SEQ_Prio_0);
}



static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
	if ((appData != NULL) && (params != NULL))
	{
		if( appData->Port == 0)
		{
			for (uint8_t i = 0 ; i < appData->BufferSize ; i++)
			{
				sprintf(debug_buffer, "%02X", appData->Buffer[i]);
				send_data_over_uart(debug_buffer);
			}
			sprintf(debug_buffer, "\r\n");
			send_data_over_uart(debug_buffer);
		}

		else
		{
			APP_LOG(TS_OFF, VLEVEL_L, "\r\n");
			APP_LOG(TS_OFF, VLEVEL_L, "- Port        %d \r\n",appData->Port);
			APP_LOG(TS_OFF, VLEVEL_L, "- Slot        RX%s \r\n",params->RxSlot);
			APP_LOG(TS_OFF, VLEVEL_L, "- Data Rate   %d \r\n",params->Datarate);
			APP_LOG(TS_OFF, VLEVEL_L, "- RSSI        %d dBm\r\n",params->Rssi);
			APP_LOG(TS_OFF, VLEVEL_L, "- SNR         %d dB\r\n",params->Snr);
		}

		switch (appData->Port)
		{
			case LORAWAN_SWITCH_CLASS_PORT:

				if (appData->BufferSize == 1)
				{
					APP_LOG_COLOR(GREEN);

					switch (appData->Buffer[0])
					{
						case 0:
							{
								LmHandlerRequestClass(CLASS_A); APP_LOG(TS_OFF, VLEVEL_L, "Switch to class A\r\n");
								break;
							}
						case 1:
							{
								LmHandlerRequestClass(CLASS_B); APP_LOG(TS_OFF, VLEVEL_L, "Switch to class B\r\n");
								break;
							}
						case 2:
							{
								LmHandlerRequestClass(CLASS_C); APP_LOG(TS_OFF, VLEVEL_L, "Switch to class C\r\n");
								break;
							}
						default:
							break;
					}
				}
				break;


			default:
				if ((appData->BufferSize >= 1) && (appData->BufferSize < 25))
				{
					memset(downlink_processing_buffer, 0 , sizeof(downlink_processing_buffer));
					memcpy(downlink_processing_buffer, appData->Buffer, appData->BufferSize);

					process_downlink_data(downlink_processing_buffer, appData->BufferSize);
				}

				break;
		}
	}
}




uint8_t flush_data_onto_cloud(void)
{
	uint8_t res = 0;
	UTIL_TIMER_Time_t nextTxIn = 0;

	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;

	if ((JoinLedTimer.IsRunning) && (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET))
	{
		res = 0;
	}

	else
	{
		status = LmHandlerSend(&AppData, LORAMAC_HANDLER_CONFIRMED_MSG, &nextTxIn, false);

		if (LORAMAC_HANDLER_SUCCESS == status)
		{
			res = 1;
		}
	}

	return res;
}



static void OnTxData(LmHandlerTxParams_t *params)
{
	uint8_t res = 0;

	if ((params != NULL))
	{
		turn_on_led(BLUE);

		device_config.battery_counter++;

		if (params->IsMcpsConfirm != 0)
		{
			if(params->AppData.Port != 0)
			{
				if (1 == params->AckReceived)
				{
					if (1 == next_action.updated_configurations_received)
					{
						next_action.updated_configurations_received = 0;

						sprintf(debug_buffer, "New Device Config ACK TX Success\r\n");
						send_data_over_uart(debug_buffer);

						UTIL_TIMER_Stop(&SampleTimer);
						UTIL_TIMER_SetPeriod(&SampleTimer, (device_config.sample_interval * 1000));
						UTIL_TIMER_Start(&SampleTimer);
					}

					else if (1 == next_action.configuration_msg_received)
					{
						next_action.configuration_msg_received = 0;

						sprintf(debug_buffer, "Device Config TX Success\r\n");
						send_data_over_uart(debug_buffer);
					}


					else if (1 == next_action.battery_caliberation_request_received)
					{
						next_action.battery_caliberation_request_received = 0;

						device_config.battery_counter = 0;

						sprintf(debug_buffer, "Battery Calibration TX Success\r\n");
						send_data_over_uart(debug_buffer);
					}

					else if (1 == next_action.erase_samples_request_received)
					{
						next_action.erase_samples_request_received = 0;
						sprintf(debug_buffer, "Erase Samples ACK TX Success\r\n");
						send_data_over_uart(debug_buffer);
					}

					else if (1 == next_action.vibration_detected)
					{
						next_action.vibration_detected = 0;
						sprintf(debug_buffer, "Vibration Message TX Success\r\n");
						send_data_over_uart(debug_buffer);
					}

					else if ((1 == next_action.energy_meter_message_flag) || (1 == next_action.periodic_message_flag) || (device_config.write_counter > device_config.read_counter))// TODO: AMP 06/03/2026
					{
						if (1 == next_action.energy_meter_message_flag)
						{
							next_action.energy_meter_message_flag = 0;
						}
						if (1 == next_action.periodic_message_flag)
						{
							next_action.periodic_message_flag = 0;
						}

						device_config.read_counter += 1;

						sprintf(debug_buffer, "ACK Received, pending_packets: %d\r\n", (device_config.write_counter - device_config.read_counter));
						send_data_over_uart(debug_buffer);
					}

				    res = 1;
				}

				else
				{
					sprintf(debug_buffer, "No ACK Received. Starting link check recovery.\r\n");
					send_data_over_uart(debug_buffer);
				}
			}
		}

		erase_config_data();
		write_config_data(&device_config);

		turn_off_led(BLUE);
	}

#if RANGE_TESTING_MODE
	if ((1 != res) && (payload_uplinks == device_status))
	{
		sprintf(debug_buffer, "No ACK (range test mode). Will retry same packet.\r\n");
		send_data_over_uart(debug_buffer);
	}
#else
	if ((1 != res) && (payload_uplinks == device_status))
	{
		can_transmit_data = 0;

		UTIL_TIMER_Stop(&TxTimer);

		UTIL_TIMER_Stop(&LinkCheckTimer);
		UTIL_TIMER_Start(&LinkCheckTimer);
	}
#endif
}



static void SendTxData(void)
{
	/* USER CODE BEGIN SendTxData_1 */
	UTIL_TIMER_Time_t nextTxIn = 0;
	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;

	AppData.Port = LORAWAN_USER_APP_PORT;

	if (LmHandlerIsBusy() == false)
	{
		device_status = payload_uplinks;

		if (1 == next_action.updated_configurations_received)
		{
			create_configuration_uplink_message(AppData.Buffer);
			AppData.BufferSize = DEVICE_CONFIGURATION_ACK_MESSAGE_SIZE;

			uint8_t res = flush_data_onto_cloud();
		}


		if (1 == next_action.configuration_msg_received)
		{
			create_configuration_uplink_message(AppData.Buffer);
			AppData.BufferSize = DEVICE_CONFIGURATION_ACK_MESSAGE_SIZE;

			uint8_t res = flush_data_onto_cloud();
		}


		if (1 == next_action.battery_caliberation_request_received)
		{
			create_battery_caliberation_ack_uplink_message(AppData.Buffer);
			AppData.BufferSize = BATTERY_CALIBRATION_ACK_MESSAGE_SIZE;

			uint8_t res = flush_data_onto_cloud();
		}


		if (1 == next_action.erase_samples_request_received)
		{
			create_erase_samples_ack_uplink_message(AppData.Buffer);
			AppData.BufferSize = ERASE_SAMPLES_ACK_MESSAGE_SIZE;

			uint8_t res = flush_data_onto_cloud();
		}

		if (1 == next_action.vibration_detected)
		{
			next_action.vibration_detected = 0;

			create_vibration_message_packet(AppData.Buffer);
			AppData.BufferSize = VIBRATION_MESSAGE_SIZE; // 7 bytes: msg id, epoch time, vibration flag, battery

			uint8_t res = flush_data_onto_cloud();

			sprintf(debug_buffer, "Vibration TX attempt (res=%d): %02X%02X%02X%02X%02X%02X%02X\r\n",
					res,
					AppData.Buffer[0], AppData.Buffer[1], AppData.Buffer[2], AppData.Buffer[3],
					AppData.Buffer[4], AppData.Buffer[5], AppData.Buffer[6]);
			send_data_over_uart(debug_buffer);
		}

		if (1 == next_action.energy_meter_message_flag)
		{
			next_action.energy_meter_message_flag = 0;

			create_energy_meter_message_packet(AppData.Buffer);
			AppData.BufferSize = ENERGY_METER_MESSAGE_SIZE;

			uint8_t res = flush_data_onto_cloud();

			if (1 == res)
			{
				sprintf(debug_buffer, "Energy Meter Message TX: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
						AppData.Buffer[0], AppData.Buffer[1], AppData.Buffer[2], AppData.Buffer[3],
						AppData.Buffer[4], AppData.Buffer[5], AppData.Buffer[6], AppData.Buffer[7],
						AppData.Buffer[8], AppData.Buffer[9], AppData.Buffer[10], AppData.Buffer[11],
						AppData.Buffer[12], AppData.Buffer[13], AppData.Buffer[14], AppData.Buffer[15],
						AppData.Buffer[16], AppData.Buffer[17], AppData.Buffer[18], AppData.Buffer[19],
						AppData.Buffer[20]);
				send_data_over_uart(debug_buffer);
			}
		}

		if (1 == next_action.periodic_message_flag)
		{
			next_action.periodic_message_flag = 0;

			create_periodic_message_packet(AppData.Buffer);
			AppData.BufferSize = PERIODIC_MESSAGE_SIZE;

			uint8_t res = flush_data_onto_cloud();

			if (1 == res)
			{
				sprintf(debug_buffer, "Periodic Message TX: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
						AppData.Buffer[0], AppData.Buffer[1], AppData.Buffer[2], AppData.Buffer[3],
						AppData.Buffer[4], AppData.Buffer[5], AppData.Buffer[6], AppData.Buffer[7],
						AppData.Buffer[8], AppData.Buffer[9], AppData.Buffer[10], AppData.Buffer[11], AppData.Buffer[12]);
				send_data_over_uart(debug_buffer);
			}
		}
	}

	else
	{
		sprintf(debug_buffer, "Message Send Fail\r\n");
		send_data_over_uart(debug_buffer);
	}

	/* USER CODE END SendTxData_1 */
}



static void OnTxTimerEvent(void *context)
{
	HAL_IWDG_Refresh(&hiwdg);

	UTIL_TIMER_Time_t nextTxIn = 0;
	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;

	if (((1 == next_action.energy_meter_message_flag) || (1 == next_action.periodic_message_flag) || (1 == next_action.configuration_msg_received) ||
			(1 == next_action.battery_caliberation_request_received) || (1 == next_action.updated_configurations_received) ||
			(1 == next_action.erase_samples_request_received) || (1 == next_action.vibration_detected))
#if !RANGE_TESTING_MODE
			&& (1 == can_transmit_data)
#endif
			)
	{
		if (LmHandlerIsBusy() == false)
		{
			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);

		}
		else
		{
			sprintf(debug_buffer, "TxTimer uplink BUSY\r\n");
			send_data_over_uart(debug_buffer);
		}
	}
}



static void OnLinkCheckTimerEvent(void *context)
{
	uint8_t res = 0;

	HAL_IWDG_Refresh(&hiwdg);

	device_status = link_check_in_progress;


    if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET)
    {
        sprintf(debug_buffer, "Link check skipped: Not joined\r\n");
        send_data_over_uart(debug_buffer);
    }

    else
    {
        LmHandlerLinkCheckReq();
        LmHandlerErrorStatus_t status = LmHandlerSend(&link_check_payload, LORAMAC_HANDLER_CONFIRMED_MSG, &nextTxIn, false);

        if (LORAMAC_HANDLER_SUCCESS == status)
        {
        	res = 1;
        }

        else
        {
            sprintf(debug_buffer, "Link check uplink failed\r\n");
            send_data_over_uart(debug_buffer);
        }
    }


    if (1 != res)
    {
    	can_transmit_data = 0;

    	transmission_failure_counter++;

        if (transmission_failure_counter >= LINK_CHECK_MAX_RETRY_ATTEMPTS)
        {
            uint32_t pending = (device_config.write_counter > device_config.read_counter)
                             ? (device_config.write_counter - device_config.read_counter) : 0;

            if ((1 == power_up) || (pending >= FLASH_FULL_THRESHOLD))
            {
                sprintf(debug_buffer, "Max retries reached (%d), pending=%lu. Rebooting.\r\n",
                        LINK_CHECK_MAX_RETRY_ATTEMPTS, (unsigned long)pending);
                send_data_over_uart(debug_buffer);

                HAL_Delay(100);

                NVIC_SystemReset();
            }
            else
            {
                sprintf(debug_buffer, "Max retries reached but flash has space (pending=%lu/%d). Waiting...\r\n",
                        (unsigned long)pending, FLASH_FULL_THRESHOLD);
                send_data_over_uart(debug_buffer);
            }
        }
    }
}


static void OnMlmeConfirm( MlmeConfirm_t *mlmeConfirm ) // TODO: AMP 21-08-2025
{
	HAL_IWDG_Refresh(&hiwdg);

    if ((mlmeConfirm->MlmeRequest == MLME_LINK_CHECK) && (mlmeConfirm->NbGateways >= 1))
    {
    	device_status = link_check_successfull;

        sprintf(debug_buffer, "LinkCheckAns OK: Margin=%d, GwCnt=%d\r\n",
                mlmeConfirm->DemodMargin, mlmeConfirm->NbGateways);
        send_data_over_uart(debug_buffer);

		UTIL_TIMER_Stop(&LinkCheckTimer);

		transmission_failure_counter = 0;
		can_transmit_data = 1;

		UTIL_TIMER_Stop(&TxTimer);
		UTIL_TIMER_Start(&TxTimer);
    }

    else
    {
    	device_status = link_check_fail;

    	can_transmit_data = 0;

    	sprintf(debug_buffer, "Link check failed, retrying in %d s\r\n", (LINK_CHECK_RETRY_INTERVAL_MS / 1000));
    	send_data_over_uart(debug_buffer);

    	UTIL_TIMER_Stop(&TxTimer);

    	transmission_failure_counter++;

    	if (transmission_failure_counter >= LINK_CHECK_MAX_RETRY_ATTEMPTS)
    	{
    		uint32_t pending = (device_config.write_counter > device_config.read_counter)
    		                 ? (device_config.write_counter - device_config.read_counter) : 0;

    		if ((1 == power_up) || (pending >= FLASH_FULL_THRESHOLD))
    		{
    			sprintf(debug_buffer, "Max retries reached (%d), pending=%lu. Rebooting.\r\n",
    			        LINK_CHECK_MAX_RETRY_ATTEMPTS, (unsigned long)pending);
    			send_data_over_uart(debug_buffer);

    			HAL_Delay(100);

    			NVIC_SystemReset();
    		}

    		else
    		{
    			sprintf(debug_buffer, "Max retries reached but flash has space (pending=%lu/%d). Waiting...\r\n",
    			        (unsigned long)pending, FLASH_FULL_THRESHOLD);
    			send_data_over_uart(debug_buffer);
    		}
    	}
    }
}


static void OnClockSyncTimerEvent(void *context)
{
	HAL_IWDG_Refresh(&hiwdg);

	device_status = time_sync_in_progress;

    if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET)
    {
        sprintf(debug_buffer, "Time sync skipped: Not joined\r\n");
        send_data_over_uart(debug_buffer);

        transmission_failure_counter++;
    }

    else if (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET)
    {
		LmHandlerErrorStatus_t res = LmhpClockSyncAppTimeReq();

		if (LORAMAC_HANDLER_SUCCESS != res)
		{
			sprintf(debug_buffer, "time synchronization request fail\r\n");
			send_data_over_uart(debug_buffer);

			transmission_failure_counter++;
		}
    }


	if (transmission_failure_counter >= LINK_CHECK_MAX_RETRY_ATTEMPTS)
	{
		uint32_t pending = (device_config.write_counter > device_config.read_counter)
		                 ? (device_config.write_counter - device_config.read_counter) : 0;

		if ((1 == power_up) || (pending >= FLASH_FULL_THRESHOLD))
		{
			sprintf(debug_buffer, "Max retries reached (%d), pending=%lu. Rebooting.\r\n",
			        LINK_CHECK_MAX_RETRY_ATTEMPTS, (unsigned long)pending);
			send_data_over_uart(debug_buffer);

			HAL_Delay(100);

			NVIC_SystemReset();
		}

		else
		{
			sprintf(debug_buffer, "Max retries reached but flash has space (pending=%lu/%d). Waiting...\r\n",
			        (unsigned long)pending, FLASH_FULL_THRESHOLD);
			send_data_over_uart(debug_buffer);
		}
	}
}


void OnSysTimeUpdate(void *context) // TODO: AMP 29-08-2025
{
	HAL_IWDG_Refresh(&hiwdg);

	SysTime_t sysTime = SysTimeGet();

	if ((sysTime.Seconds > JANUARY_1_2025_EPOCH) && (sysTime.Seconds < JANUARY_1_2050_EPOCH))
	{
		device_status = time_sync_successfull;

		transmission_failure_counter = 0;

		sprintf(debug_buffer, "epoch_time: %lu\r\n", sysTime.Seconds);
		send_data_over_uart(debug_buffer);

		if (1 == power_up)
		{
			power_up = 0;
			can_transmit_data = 1;

			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_RunApplicationLogic), CFG_SEQ_Prio_0);

			UTIL_TIMER_Stop(&SampleTimer);
			UTIL_TIMER_Start(&SampleTimer);

			UTIL_TIMER_Stop(&TxTimer);
			UTIL_TIMER_Start(&TxTimer);

			UTIL_TIMER_Stop(&ClockSyncTimer);
			UTIL_TIMER_StartWithPeriod(&ClockSyncTimer, 28800 * 1000); // 8 hours
		}
	}

	else
	{
		device_status = time_sync_fail;

		transmission_failure_counter++;
	}


	if (transmission_failure_counter >= LINK_CHECK_MAX_RETRY_ATTEMPTS)
	{
		uint32_t pending = (device_config.write_counter > device_config.read_counter)
		                 ? (device_config.write_counter - device_config.read_counter) : 0;

		if ((1 == power_up) || (pending >= FLASH_FULL_THRESHOLD))
		{
			sprintf(debug_buffer, "Max retries reached (%d), pending=%lu. Rebooting.\r\n",
			        LINK_CHECK_MAX_RETRY_ATTEMPTS, (unsigned long)pending);
			send_data_over_uart(debug_buffer);

			HAL_Delay(100);

			NVIC_SystemReset();
		}

		else
		{
			sprintf(debug_buffer, "Max retries reached but flash has space (pending=%lu/%d). Waiting...\r\n",
			        (unsigned long)pending, FLASH_FULL_THRESHOLD);
			send_data_over_uart(debug_buffer);
		}
	}
}


static void OnIWDGRefreshTimerEvent(void *context)
{
	HAL_IWDG_Refresh(&hiwdg);
}


static void OnTxTimerLedEvent(void *context)
{
	BSP_LED_Off(LED_GREEN) ;
}

static void OnRxTimerLedEvent(void *context)
{
	BSP_LED_Off(LED_BLUE) ;
}

static void OnJoinTimerLedEvent(void *context)
{
	BSP_LED_Toggle(LED_RED) ;
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
	case USER_SW_Pin:
		sw_pressed = 1;
		__HAL_GPIO_EXTI_CLEAR_IT(USER_SW_Pin);
		UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_CheckUserSwitchStatus), CFG_SEQ_Prio_0);
		break;
	case VIBRATION_SENSOR_Pin:
	{
		/* Sustained-Pulse Verification:
		 * Rejects sub-microsecond comparator chattering and noise.
		 * Samples the pin 500 times over ~1ms. A genuine physical vibration
		 * will hold the line LOW for multiple samples. */
		static uint32_t last_vibration_alert_tick = 0;
		uint32_t now = HAL_GetTick();

		uint16_t low_samples = 0;
		for (volatile int i = 0; i < 500; i++)
		{
			if (HAL_GPIO_ReadPin(VIBRATION_SENSOR_GPIO_Port, VIBRATION_SENSOR_Pin) == GPIO_PIN_RESET)
			{
				low_samples++;
			}
		}

		// Only trigger if at least 50% of samples confirm the line is actively pulled LOW
		if (low_samples > 250)
		{
			if ((now - last_vibration_alert_tick) >= VIBRATION_COOLDOWN_MS)
			{
				last_vibration_alert_tick = now;
				next_action.vibration_detected = 1;
				UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
			}
		}

		__HAL_GPIO_EXTI_CLEAR_IT(VIBRATION_SENSOR_Pin);
		break;
	}
	default:
		break;
	}
}
