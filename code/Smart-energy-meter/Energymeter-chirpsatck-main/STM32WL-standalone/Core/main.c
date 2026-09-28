
#include "main.h"
#include "app_lorawan.h"
#include "stm32wlxx_hal_spi.h"
#include "lora_app.h"
#include "sys_app.h"
#include "stm32_seq.h"


#include "gpio.h"
#include "usart.h"
#include "eeprom.h"
#include "application_logic.h"
#include "pzem004t.h"


IWDG_HandleTypeDef hiwdg; // TODO: AMP 29/08/2025



void SystemClock_Config(void);
void MX_GPIO_Init(void);
void IWDG_Init(void); // TODO: AMP 29/08/2025
void MX_SPI1_Init(void);
void MX_TIM2_Init(void);

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;


void reset_cause_get(void)
{
    uint32_t reset_cause = RCC->CSR;

	if (reset_cause & RCC_CSR_IWDGRSTF)
	{
		sprintf(debug_buffer, "IWDG Reset (Independent Watchdog timeout)\r\n");
	}
	else if (reset_cause & RCC_CSR_WWDGRSTF)
	{
		sprintf(debug_buffer, "WWDG Reset (Window Watchdog)\r\n");
	}
	else if (reset_cause & RCC_CSR_SFTRSTF)
	{
		sprintf(debug_buffer, "Software Reset\r\n");
	}
	else if (reset_cause & RCC_CSR_PINRSTF)
	{
		sprintf(debug_buffer, "NRST Pin Reset\r\n");
	}
	else if (reset_cause & RCC_CSR_BORRSTF)
	{
		sprintf(debug_buffer, "Brownout Reset\r\n");
	}
	else
	{
		sprintf(debug_buffer, "Unknown or No Specific Reset Flag\r\n");
	}

	send_data_over_uart(debug_buffer);

	// Clear all reset flags by setting RMVF (bit 23)
	RCC->CSR |= RCC_CSR_RMVF;

}



int main(void)
{

	HAL_Init();
	SystemClock_Config();

	/* Initialize USART1 (debug UART on PB6/PB7) once - stays open permanently */
	MX_USART1_UART_Init();

	reset_cause_get();

	IWDG_Init();

	HAL_IWDG_Refresh(&hiwdg);
	memset(&write_sensor_data, 0, sizeof(write_sensor_data));

	HAL_IWDG_Refresh(&hiwdg);
	init_sample_storage();

	sprintf(debug_buffer, "device_config.new_program: %02X, device_config.sample_interval: %d, "
			"device_config.upload_interval: %d, device_config.write_counter: %d, device_config.read_counter: %d, "
			"device_config.battery_counter: %d\r\n",
			device_config.new_program, device_config.sample_interval, device_config.upload_interval,
			device_config.write_counter, device_config.read_counter, device_config.battery_counter);
	send_data_over_uart(debug_buffer);

	HAL_IWDG_Refresh(&hiwdg);
	MX_GPIO_Init();

	/* Initialize USART2 for PZEM-004T (PA2=TX2, PA3=RX2, 9600 baud, 8N1) */
	MX_USART2_UART_Init();
	send_data_over_uart("PZEM USART2 Init OK\r\n");

	HAL_IWDG_Refresh(&hiwdg);
	SystemApp_Init();

	HAL_IWDG_Refresh(&hiwdg);
	turn_on_led(RED);
	HAL_Delay(1000);
	HAL_IWDG_Refresh(&hiwdg);
	turn_off_led(RED);

	HAL_IWDG_Refresh(&hiwdg);
	LoRaWAN_Init();
	send_data_over_uart("LoRaWAN Init OK\r\n");


	while (1)
	{
		HAL_IWDG_Refresh(&hiwdg);
		MX_LoRaWAN_Process();
	}
}


void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure LSE Drive Capability
	 */
	HAL_PWR_EnableBkUpAccess();
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
	RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1; // TODO: AMP 08/09/2025
	RCC_OscInitStruct.LSIState = RCC_LSI_ON; // TODO: AMP 08/09/2025
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
	/** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
			|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
			|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}


void IWDG_Init(void) // TODO: AMP 29/08/2025
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 4095;
    hiwdg.Init.Window = 4095;

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        Error_Handler(); // Call error handler if initialization fails
    }
}


/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
void MX_SPI1_Init(void)
{
//  /* SPI1 parameter configuration*/
//  hspi1.Instance = SPI1;
//  hspi1.Init.Mode = SPI_MODE_MASTER;
//  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
//  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
//  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
//  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
//  hspi1.Init.NSS = SPI_NSS_SOFT;
//  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
//  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
//  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
//  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
//  hspi1.Init.CRCPolynomial = 7;
//  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
//  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
//  if (HAL_SPI_Init(&hspi1) != HAL_OK)
//  {
//    Error_Handler();
//  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
//void MX_GPIO_Init(void)
//{
//  /* GPIO Ports Clock Enable */
//  __HAL_RCC_GPIOC_CLK_ENABLE();
//  __HAL_RCC_GPIOA_CLK_ENABLE();
//  __HAL_RCC_GPIOB_CLK_ENABLE();
//
//	GPIO_InitTypeDef CS_init;
//	CS_init.Mode = GPIO_MODE_OUTPUT_PP;
//	CS_init.Pin = GPIO_PIN_2;
//	CS_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//	HAL_GPIO_Init(GPIOC, &CS_init);
//
//	GPIO_InitTypeDef RST_init;
//	RST_init.Mode = GPIO_MODE_OUTPUT_PP;
//	RST_init.Pin = GPIO_PIN_2;
//	RST_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//	HAL_GPIO_Init(GPIOB, &RST_init);
//
//	GPIO_InitTypeDef DC_init;
//	DC_init.Mode = GPIO_MODE_OUTPUT_PP;
//	DC_init.Pin = GPIO_PIN_10;
//	DC_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//	HAL_GPIO_Init(GPIOB, &DC_init);
//}


void MX_TIM2_Init(void)
{
  __HAL_RCC_TIM2_CLK_ENABLE();  // Enable clock for TIM2
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 48-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 0xFFFFFFFF;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}



void Error_Handler(void)
{
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
}
