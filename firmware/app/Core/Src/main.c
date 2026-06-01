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

typedef enum
{
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_PIN,
    RESET_CAUSE_POR,
    RESET_CAUSE_BOR,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_IWDG,
    RESET_CAUSE_WWDG
} ResetCause_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ENABLE_SOFTWARE_RESET_TEST   0
#define SOFTWARE_RESET_DELAY_MS      5000U

#define ENABLE_HARDFAULT_TEST        0
#define HARDFAULT_TEST_DELAY_MS      5000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t g_reset_from_software = 0U;
static ResetCause_t g_primary_reset_cause = RESET_CAUSE_UNKNOWN;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Print_HFSR_Decode(uint32_t hfsr)
{
    printf("\r\n");
    printf("HFSR Decode:\r\n");

    if ((hfsr & (1UL << 1)) != 0U)
    {
        printf("  - VECTTBL : Bus fault on vector table read\r\n");
    }

    if ((hfsr & (1UL << 30)) != 0U)
    {
        printf("  - FORCED  : Configurable fault escalated to HardFault\r\n");
    }

    if ((hfsr & (1UL << 31)) != 0U)
    {
        printf("  - DEBUGEVT: Debug event occurred\r\n");
    }

    if (hfsr == 0U)
    {
        printf("  - None\r\n");
    }
}
static void Print_CFSR_Decode(uint32_t cfsr)
{
    uint32_t mmfsr = (cfsr & 0x000000FFUL);
    uint32_t bfsr  = (cfsr & 0x0000FF00UL) >> 8;
    uint32_t ufsr  = (cfsr & 0xFFFF0000UL) >> 16;

    printf("\r\n");
    printf("CFSR Decode:\r\n");

    printf("  MMFSR = 0x%02lX\r\n", mmfsr);
    printf("  BFSR  = 0x%02lX\r\n", bfsr);
    printf("  UFSR  = 0x%04lX\r\n", ufsr);

    printf("\r\n");
    printf("MemManage Fault:\r\n");

    if ((cfsr & (1UL << 0)) != 0U)
    {
        printf("  - IACCVIOL : Instruction access violation\r\n");
    }

    if ((cfsr & (1UL << 1)) != 0U)
    {
        printf("  - DACCVIOL : Data access violation\r\n");
    }

    if ((cfsr & (1UL << 3)) != 0U)
    {
        printf("  - MUNSTKERR: MemManage fault on exception return unstacking\r\n");
    }

    if ((cfsr & (1UL << 4)) != 0U)
    {
        printf("  - MSTKERR  : MemManage fault on exception entry stacking\r\n");
    }

    if ((cfsr & (1UL << 5)) != 0U)
    {
        printf("  - MLSPERR  : MemManage fault during lazy FP state preservation\r\n");
    }

    if ((cfsr & (1UL << 7)) != 0U)
    {
        printf("  - MMARVALID: MMFAR holds a valid fault address\r\n");
    }

    if (mmfsr == 0U)
    {
        printf("  - None\r\n");
    }

    printf("\r\n");
    printf("BusFault:\r\n");

    if ((cfsr & (1UL << 8)) != 0U)
    {
        printf("  - IBUSERR   : Instruction bus error\r\n");
    }

    if ((cfsr & (1UL << 9)) != 0U)
    {
        printf("  - PRECISERR : Precise data bus error\r\n");
    }

    if ((cfsr & (1UL << 10)) != 0U)
    {
        printf("  - IMPRECISERR: Imprecise data bus error\r\n");
    }

    if ((cfsr & (1UL << 11)) != 0U)
    {
        printf("  - UNSTKERR  : BusFault on exception return unstacking\r\n");
    }

    if ((cfsr & (1UL << 12)) != 0U)
    {
        printf("  - STKERR    : BusFault on exception entry stacking\r\n");
    }

    if ((cfsr & (1UL << 13)) != 0U)
    {
        printf("  - LSPERR    : BusFault during lazy FP state preservation\r\n");
    }

    if ((cfsr & (1UL << 15)) != 0U)
    {
        printf("  - BFARVALID : BFAR holds a valid fault address\r\n");
    }

    if (bfsr == 0U)
    {
        printf("  - None\r\n");
    }

    printf("\r\n");
    printf("UsageFault:\r\n");

    if ((cfsr & (1UL << 16)) != 0U)
    {
        printf("  - UNDEFINSTR: Undefined instruction\r\n");
    }

    if ((cfsr & (1UL << 17)) != 0U)
    {
        printf("  - INVSTATE  : Invalid EPSR/T-bit state\r\n");
    }

    if ((cfsr & (1UL << 18)) != 0U)
    {
        printf("  - INVPC     : Invalid PC load / EXC_RETURN\r\n");
    }

    if ((cfsr & (1UL << 19)) != 0U)
    {
        printf("  - NOCP      : No coprocessor\r\n");
    }

    if ((cfsr & (1UL << 24)) != 0U)
    {
        printf("  - UNALIGNED : Unaligned memory access\r\n");
    }

    if ((cfsr & (1UL << 25)) != 0U)
    {
        printf("  - DIVBYZERO : Divide by zero\r\n");
    }

    if (ufsr == 0U)
    {
        printf("  - None\r\n");
    }
}
void HardFault_Handler_C(uint32_t *stack_frame)
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;

    r0   = stack_frame[0];
    r1   = stack_frame[1];
    r2   = stack_frame[2];
    r3   = stack_frame[3];
    r12  = stack_frame[4];
    lr   = stack_frame[5];
    pc   = stack_frame[6];
    xpsr = stack_frame[7];

    printf("\r\n");
    printf("========== HardFault ==========\r\n");
    printf("R0   = 0x%08lX\r\n", r0);
    printf("R1   = 0x%08lX\r\n", r1);
    printf("R2   = 0x%08lX\r\n", r2);
    printf("R3   = 0x%08lX\r\n", r3);
    printf("R12  = 0x%08lX\r\n", r12);
    printf("LR   = 0x%08lX\r\n", lr);
    printf("PC   = 0x%08lX\r\n", pc);
    printf("xPSR = 0x%08lX\r\n", xpsr);

    printf("\r\n");
    printf("CFSR = 0x%08lX\r\n", SCB->CFSR);
    printf("HFSR = 0x%08lX\r\n", SCB->HFSR);
    printf("DFSR = 0x%08lX\r\n", SCB->DFSR);
    printf("AFSR = 0x%08lX\r\n", SCB->AFSR);
    printf("MMFAR= 0x%08lX\r\n", SCB->MMFAR);
    printf("BFAR = 0x%08lX\r\n", SCB->BFAR);
		
		Print_HFSR_Decode(SCB->HFSR);
		Print_CFSR_Decode(SCB->CFSR);
		
    printf("================================\r\n");

    /*
     * Stay here for debugging.
     * In future, this info should be stored into blackbox before reset.
     */
    while (1)
    {
    }
}


static const char *ResetCause_ToString(ResetCause_t cause)
{
    switch (cause)
    {
        case RESET_CAUSE_PIN:
            return "Pin Reset";

        case RESET_CAUSE_POR:
            return "Power On / Power Down Reset";

        case RESET_CAUSE_BOR:
            return "Brown-out Reset";

        case RESET_CAUSE_SOFTWARE:
            return "Software Reset";

        case RESET_CAUSE_IWDG:
            return "Independent Watchdog Reset";

        case RESET_CAUSE_WWDG:
            return "Window Watchdog Reset";

        case RESET_CAUSE_UNKNOWN:
        default:
            return "Unknown Reset";
    }
}

static ResetCause_t Detect_PrimaryResetCause(void)
{
    /*
     * Reset flags are not mutually exclusive.
     * PINRST may be set together with other reset flags.
     *
     * Therefore, do not treat PINRST as the highest-priority cause.
     * Watchdog and software reset are usually more meaningful for diagnostics.
     */

#ifdef RCC_FLAG_IWDG1RST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST) != 0U)
    {
        return RESET_CAUSE_IWDG;
    }
#endif

#ifdef RCC_FLAG_WWDG1RST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDG1RST) != 0U)
    {
        return RESET_CAUSE_WWDG;
    }
#endif

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0U)
    {
        return RESET_CAUSE_SOFTWARE;
    }

#ifdef RCC_FLAG_BORRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != 0U)
    {
        return RESET_CAUSE_BOR;
    }
#endif

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0U)
    {
        return RESET_CAUSE_POR;
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0U)
    {
        return RESET_CAUSE_PIN;
    }

    return RESET_CAUSE_UNKNOWN;
}

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
    g_primary_reset_cause = Detect_PrimaryResetCause();

    if (g_primary_reset_cause == RESET_CAUSE_SOFTWARE)
    {
        g_reset_from_software = 1U;
    }
    else
    {
        g_reset_from_software = 0U;
    }

    printf("----------------------------------------\r\n");
    printf(" Reset Flags:\r\n");

    printf("  PINRST  : %s\r\n",
           (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0U) ? "SET" : "RESET");

    printf("  PORRST  : %s\r\n",
           (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0U) ? "SET" : "RESET");

#ifdef RCC_FLAG_BORRST
    printf("  BORRST  : %s\r\n",
           (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != 0U) ? "SET" : "RESET");
#endif

    printf("  SFTRST  : %s\r\n",
           (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0U) ? "SET" : "RESET");

#ifdef RCC_FLAG_IWDG1RST
    printf("  IWDGRST : %s\r\n",
           (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST) != 0U) ? "SET" : "RESET");
#endif

#ifdef RCC_FLAG_WWDG1RST
    printf("  WWDGRST : %s\r\n",
           (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDG1RST) != 0U) ? "SET" : "RESET");
#endif

    printf("\r\n");
    printf(" Primary Reset Cause: %s\r\n",
           ResetCause_ToString(g_primary_reset_cause));

    /*
     * Clear reset flags after reading and printing.
     * Reset flags are sticky and may affect the next boot diagnosis.
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

#if ENABLE_SOFTWARE_RESET_TEST
    if ((g_reset_from_software == 0U) && (HAL_GetTick() > SOFTWARE_RESET_DELAY_MS))
    {
        printf("[RESET_TEST] Trigger software reset by NVIC_SystemReset()\r\n");
        HAL_Delay(100);
        NVIC_SystemReset();
    }
#endif
		
#if ENABLE_HARDFAULT_TEST
    if (HAL_GetTick() > HARDFAULT_TEST_DELAY_MS)
    {
        printf("[FAULT_TEST] Trigger HardFault test\r\n");
        HAL_Delay(100);

        /*
         * Trigger a fault by writing to an invalid address.
         * This is only for bring-up test.
         */
        volatile uint32_t *bad_addr = (uint32_t *)0xFFFFFFFFU;
        *bad_addr = 0x12345678U;
    }
#endif
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
