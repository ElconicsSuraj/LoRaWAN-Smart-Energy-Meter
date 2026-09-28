

#include "main.h"
#include "stm32wlxx_it.h"
#include "sys_app.h"
//#include "st7789.h"

extern RTC_HandleTypeDef hrtc;
extern SUBGHZ_HandleTypeDef hsubghz;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef huart1;

void NMI_Handler(void)
{
  while (1)
  {
  }
}


void HardFault_Handler(void)
{

  while (1)
  {

  }
}


void MemManage_Handler(void)
{

  while (1)
  {

  }
}


void BusFault_Handler(void)
{

  while (1)
  {

  }
}


void UsageFault_Handler(void)
{

  while (1)
  {

  }
}


void SVC_Handler(void)
{

}


void DebugMon_Handler(void)
{

}


void PendSV_Handler(void)
{

}


void SysTick_Handler(void)
{

}

/******************************************************************************/
/* STM32WLxx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32wlxx.s).                    */
/******************************************************************************/


void TAMP_STAMP_LSECSS_SSRU_IRQHandler(void)
{

  HAL_RTCEx_SSRUIRQHandler(&hrtc);

}


void EXTI0_IRQHandler(void)
{

  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);

}


void EXTI1_IRQHandler(void)
{

  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);

}

void EXTI3_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(VIBRATION_SENSOR_Pin);
}

////PROJECT BUTTON LCD
//void EXTI9_5_IRQHandler(void)
//{
//
//  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
//  APP_LOG(0, 1, "Button pressed!\r\n");
//
//  char * string = "Button pressed!";
//  ST7789_Fill_Color(LCD_WHITE);
//  ST7789_WriteString(20, 20, string, Font_11x18, LCD_BRED, LCD_WHITE);
//
//}
////PROJECT BUTTON LCD END

void DMA1_Channel5_IRQHandler(void)
{

  HAL_DMA_IRQHandler(&hdma_usart1_tx);

}


void USART1_IRQHandler(void)
{

  HAL_UART_IRQHandler(&huart1);

}


void RTC_Alarm_IRQHandler(void)
{

  HAL_RTC_AlarmIRQHandler(&hrtc);

}


void SUBGHZ_Radio_IRQHandler(void)
{

  HAL_SUBGHZ_IRQHandler(&hsubghz);

}


void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(USER_SW_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

