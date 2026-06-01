/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);
    return ch;
}

int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);
    return ch;
}
static void Print_ResetReason(void)
{
    printf("----------------------------------------\r\n");
    printf(" Reset Reason:\r\n");

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0U)
    {
        printf("  - PIN Reset: Yes\r\n");
    }
    else
    {
        printf("  - PIN Reset: No\r\n");
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0U)
    {
        printf("  - POR/PDR Reset: Yes\r\n");
    }
    else
    {
        printf("  - POR/PDR Reset: No\r\n");
    }

#ifdef RCC_FLAG_BORRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != 0U)
    {
        printf("  - BOR Reset: Yes\r\n");
    }
    else
    {
        printf("  - BOR Reset: No\r\n");
    }
#endif

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0U)
    {
        printf("  - Software Reset: Yes\r\n");
    }
    else
    {
        printf("  - Software Reset: No\r\n");
    }

#ifdef RCC_FLAG_IWDG1RST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST) != 0U)
    {
        printf("  - IWDG Reset: Yes\r\n");
    }
    else
    {
        printf("  - IWDG Reset: No\r\n");
    }
#endif

#ifdef RCC_FLAG_WWDG1RST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDG1RST) != 0U)
    {
        printf("  - WWDG Reset: Yes\r\n");
    }
    else
    {
        printf("  - WWDG Reset: No\r\n");
    }
#endif

    /*
     * Clear reset flags after printing.
     * Otherwise the same reset reason may remain visible after next reset.
     */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

static void Print_ClockInfo(void)
{
    printf("----------------------------------------\r\n");
    printf(" Clock Info:\r\n");
    printf("  SYSCLK = %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("  HCLK   = %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    printf("  PCLK1  = %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());
    printf("  PCLK2  = %lu Hz\r\n", HAL_RCC_GetPCLK2Freq());
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
/* USER CODE BEGIN 2 */
	HAL_UART_Transmit(&huart1, (uint8_t *)"raw uart ok\r\n", 13, 100);

	printf("\r\n");
	printf("========================================\r\n");
	printf(" DM-MC02 H723 Embedded Framework\r\n");
	printf(" Stage 1: Board Bring-up\r\n");
	printf(" MCU: STM32H723VGT6\r\n");
	printf(" UART: USART1 115200 8N1\r\n");
	printf(" Build: %s %s\r\n", __DATE__, __TIME__);

	Print_ResetReason();
	Print_ClockInfo();

	printf("----------------------------------------\r\n");
	printf(" Tick Test:\r\n");
	printf("  HAL_GetTick = %lu ms\r\n", HAL_GetTick());
	printf("========================================\r\n");
/* USER CODE END 2 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    printf("[BOOT] uptime = %lu ms\r\n", HAL_GetTick());
    HAL_Delay(1000);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
