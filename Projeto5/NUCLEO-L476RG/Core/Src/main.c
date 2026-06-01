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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Endereços I2C do PCF8591
#define PCF8591_ADDR (0x48 << 1)

// Endereços dos Registradores do MAX7219
#define REG_DECODE_MODE  0x09
#define REG_INTENSITY    0x0A
#define REG_SCAN_LIMIT   0x0B
#define REG_SHUTDOWN     0x0C
#define REG_DISPLAY_TEST 0x0F

// Variáveis UART e I2C (já do seu código anterior)
uint8_t rx_char;
char rx_buffer[50];
volatile uint8_t rx_index = 0;
volatile uint8_t comando_pronto = 0;

uint8_t i2c_tx_buffer[2];
uint8_t i2c_rx_buffer[2];
volatile uint8_t i2c_read_done = 0;

// Variáveis do Sistema
typedef enum { NENHUM, TEMP, VOLT, LDR } Sensor_Ativo;
volatile Sensor_Ativo sensor_atual = NENHUM;
volatile uint8_t valor_sensor = 0;
volatile uint8_t toggle_display = 0;
uint16_t spi_tx_buffer; // Buffer de 16 bits para a SPI

// Padrões de Bits para a Matriz (Exemplo: Letra T e Sinal de Menos)
const uint8_t char_T[8]     = {0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};
const uint8_t char_Minus[8] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00};
const uint8_t char_Plus[8]  = {0x00, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x00};

// Função para enviar comando SPI não-bloqueante para o MAX7219
void MAX7219_Write_IT(uint8_t reg, uint8_t data) {
    spi_tx_buffer = (reg << 8) | data; // Junta endereço e dado
    HAL_GPIO_WritePin(CS_SPI_GPIO_Port, CS_SPI_Pin, GPIO_PIN_RESET); // Abaixa o CS
    HAL_SPI_Transmit_IT(&hspi1, (uint8_t*)&spi_tx_buffer, 1);
}

// Função para desenhar um array de 8 bytes na matriz
void Desenhar_Matriz(const uint8_t *caractere) {
    // O ideal seria usar uma fila/buffer circular para transmissões SPI seguidas via interrupção,
    // mas como a exigência é apenas a alternância a cada 500ms, podemos engatilhar a transmissão
    // das linhas com pequenos intervalos ou via polling rápido apenas para o display.
    // Para simplificar a lógica não-bloqueante pesada, faremos uma varredura:
    for (uint8_t i = 1; i <= 8; i++) {
        MAX7219_Write_IT(i, caractere[i-1]);
        HAL_Delay(1); // Pequeno atraso para dar tempo da interrupção SPI terminar
    }
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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM6_Init();
  	/* USER CODE BEGIN 2 */
    // Inicializa o MAX7219 no modo Matriz (No-Decode)
    MAX7219_Write_IT(REG_SHUTDOWN, 0x01);     // Acorda o chip
    MAX7219_Write_IT(REG_DECODE_MODE, 0x00);  // Sem decodificador BCD
    MAX7219_Write_IT(REG_SCAN_LIMIT, 0x07);   // 8 colunas ativas
    MAX7219_Write_IT(REG_INTENSITY, 0x07);    // Brilho médio
    MAX7219_Write_IT(REG_DISPLAY_TEST, 0x00); // Teste desligado

    // Inicia Timer e UART
    HAL_TIM_Base_Start_IT(&htim6);
    HAL_UART_Receive_IT(&huart2, &rx_char, 1);
    /* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  /* USER CODE BEGIN 3 */
	      if (comando_pronto == 1) {
	          comando_pronto = 0;

	          // Processa os novos comandos exigidos pelo projeto
	          if (strncmp((char*)rx_buffer, "Temp", 4) == 0) {
	              sensor_atual = TEMP;
	              // Engatilha leitura I2C do AIN0 (Temperatura)
	              i2c_tx_buffer[0] = 0x40; // Control Byte: AIN0
	              HAL_I2C_Master_Transmit_IT(&hi2c1, PCF8591_ADDR, i2c_tx_buffer, 1);
	          }
	          else if (strncmp((char*)rx_buffer, "Volt", 4) == 0) {
	              sensor_atual = VOLT;
	              // Engatilha leitura I2C do AIN1 (Tensão)
	              i2c_tx_buffer[0] = 0x41; // Control Byte: AIN1
	              HAL_I2C_Master_Transmit_IT(&hi2c1, PCF8591_ADDR, i2c_tx_buffer, 1);
	          }
	          else if (strncmp((char*)rx_buffer, "LDR", 3) == 0) {
	              sensor_atual = LDR;
	              // Engatilha leitura I2C do AIN3 (Luminosidade)
	              i2c_tx_buffer[0] = 0x43; // Control Byte: AIN3
	              HAL_I2C_Master_Transmit_IT(&hi2c1, PCF8591_ADDR, i2c_tx_buffer, 1);
	          }

	          // Limpa o buffer da UART para o próximo comando
	          memset((char*)rx_buffer, 0, sizeof(rx_buffer));
	          rx_index = 0;
	      }

	      // Quando a interrupção do I2C terminar de receber os 2 bytes do sensor:
	      if (i2c_read_done == 1) {
	          i2c_read_done = 0;

	          // Atualiza a variável global que o Timer de 500ms está lendo
	          valor_sensor = i2c_rx_buffer[1];

	          // Imprime o valor no terminal virtual como feedback
	          char msg[40];
	          sprintf(msg, "Valor lido (%d): %d\r\n", sensor_atual, valor_sensor);
	          HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
	      }
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 7999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 4999;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_SPI_GPIO_Port, CS_SPI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_SPI_Pin */
  GPIO_InitStruct.Pin = CS_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_SPI_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// 1. Callback da UART (Recepção de Comandos do PC)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // Se recebeu Enter (Quebra de linha)
        if (rx_char == '\n' || rx_char == '\r') {
            comando_pronto = 1;
        } else {
            // Guarda a letra digitada no buffer
            rx_buffer[rx_index++] = rx_char;
        }
        // Rearma a interrupção para a próxima letra
        HAL_UART_Receive_IT(&huart2, &rx_char, 1);
    }
}

// 2. Callback da SPI (Quando termina de enviar para a Matriz)
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        // Sobe o CS para o MAX7219 "travar" o dado que acabou de chegar
        HAL_GPIO_WritePin(CS_SPI_GPIO_Port, CS_SPI_Pin, GPIO_PIN_SET);
    }
}

// 3. Callback do Timer (Piscar a matriz a cada 500ms)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM6) {
        toggle_display = !toggle_display;

        if (sensor_atual == TEMP) {
            if (valor_sensor < 128) {
                Desenhar_Matriz(toggle_display ? char_T : char_Minus);
            } else {
                Desenhar_Matriz(toggle_display ? char_T : char_Plus);
            }
        }
        // Depois você pode adicionar os ifs para VOLT (char_V) e LDR (char_L) aqui
    }
}
// 4. Callback de Fim de Transmissão I2C (Engatilha a Leitura)
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) {
        // Terminou de dizer ao sensor qual canal quer ler.
        // Agora, pede para o sensor enviar os 2 bytes de volta!
        HAL_I2C_Master_Receive_IT(&hi2c1, PCF8591_ADDR, i2c_rx_buffer, 2);
    }
}

// 5. Callback de Fim de Recepção I2C (Avisa que terminou)
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) {
        // Os dados chegaram! Levanta a bandeira para o while(1) imprimir.
        i2c_read_done = 1;
    }
}
// Callback chamada automaticamente se der erro no I2C (ex: sensor desconectado)
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) {
        // Coloque um breakpoint na linha abaixo!
        __NOP(); // Instrução vazia (No Operation) só para segurar o breakpoint
    }
}

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
#ifdef USE_FULL_ASSERT
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
