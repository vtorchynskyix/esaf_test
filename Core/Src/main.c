#include "main.h"
#include "esad.h"

#include "lsm6ds3_reg.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"

//#define DEBUG

ADC_HandleTypeDef  hadc1;
SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
void adc_select_batt(void);

static uint8_t chip_id= 0;
static stmdev_ctx_t dev_ctx;
static uint8_t response[3] = {0};
uint8_t c1;
uint8_t c2;

void drop_one_byte() { HAL_UART_Receive_IT(&huart1, &c1, 1); }

static int32_t platform_write(SPI_HandleTypeDef *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len)
{
    /* Write multiple command */
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &reg, 1, 1000);
    HAL_SPI_Transmit(handle, (uint8_t *)bufp, len, 1000);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    return 0;
}

static int32_t platform_read(SPI_HandleTypeDef *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
    reg |= 0x80;
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &reg, 1, 1000);
    HAL_SPI_Receive(handle, bufp, len, 1000);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

    return 0;
}

enum blasting_test {
    CAP_OK = 0,
    CAP_NOT_CONNECTED = 1,
    CAP_SHORTED = 2,
};

uint16_t adc_to_mv(int adcValue)
{
    const double referenceVoltage = 3.3;  // Reference voltage in volts
    const int adcMaxValue = 4095;         // Maximum ADC value for 12-bit resolution
    const double voltagePerStep = (referenceVoltage * 1000) / adcMaxValue;  // Voltage per step in mV
    return (uint16_t)((adcValue * voltagePerStep) * 2.98);
}

static uint8_t blasting_cap_check()
{
    uint8_t status = CAP_OK;
    uint8_t cap = 0;
    //Detect if shunt is present
    HAL_GPIO_WritePin(CAP_OUT_GPIO_Port, CAP_OUT_Pin, GPIO_PIN_SET);
    for (uint32_t i = 0; i < 10000; i++);;
    uint8_t shunt = HAL_GPIO_ReadPin(SHUNT_POS_GPIO_Port, SHUNT_POS_Pin);
    //HAL_GPIO_WritePin(CAP_OUT_GPIO_Port, CAP_OUT_Pin, GPIO_PIN_RESET);
    if (!shunt) {
        return CAP_SHORTED;
    } else {
        HAL_GPIO_WritePin(CAP_OUT_GPIO_Port, CAP_OUT_Pin, GPIO_PIN_SET);
        cap = HAL_GPIO_ReadPin(CAP_IN_GPIO_Port, CAP_IN_Pin);
        //HAL_GPIO_WritePin(CAP_OUT_GPIO_Port, CAP_OUT_Pin, GPIO_PIN_RESET);
    }
    if (cap) {
        return CAP_OK;
    } else {
        return CAP_NOT_CONNECTED;
    }
    return status;
}

void adc_select_batt(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_16;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

void process_command(uint8_t cmd)
{
    /* --------- Decode command bits --------- */

    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
                      (cmd & (1 << 0)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin,
                      (cmd & (1 << 1)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin,
                      (cmd & (1 << 2)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(FIRE1_GPIO_Port, FIRE1_Pin,
                      (cmd & (1 << 3)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* --------- Prepare response --------- */

    /* Byte 0 — accelerometer ID */
    lsm6ds3_device_id_get(&dev_ctx, &chip_id);
    response[0] = chip_id;

    /* Byte 1 — ADC integer voltage */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_select_batt();
    uint16_t adc_raw = HAL_ADC_GetValue(&hadc1);
    uint16_t mv = adc_to_mv(adc_raw);
    response[1] = (uint8_t)((mv / 1000) * 2);  // integer volts

    /* Byte 2 — status bits */
    uint8_t status = 0;

    uint8_t cap_status = blasting_cap_check();

    if (cap_status == CAP_OK)
        status |= (1 << 0);

    if (HAL_GPIO_ReadPin(SHUNT_POS_GPIO_Port, SHUNT_POS_Pin))
        status |= (1 << 1);

    if (!HAL_GPIO_ReadPin(SIG_1_GPIO_Port, SIG_Pin_1))
        status |= (1 << 2);

    if (!HAL_GPIO_ReadPin(SIG_2_GPIO_Port, SIG_Pin_2))
        status |= (1 << 3);

    if (!HAL_GPIO_ReadPin(SIG_3_GPIO_Port, SIG_Pin_3))
        status |= (1 << 4);

    if (!HAL_GPIO_ReadPin(SIG_4_GPIO_Port, SIG_Pin_4))
        status |= (1 << 5);

    if (!HAL_GPIO_ReadPin(SIG_5_GPIO_Port, SIG_Pin_5))
        status |= (1 << 6);

    response[2] = status;

    /* --------- Send response --------- */
    HAL_UART_Transmit_IT(&huart1, response, 3);
}

int _kill(int, int) { return -1; }
int _getpid() { return 1; }
void _exit(int) { while(1) { } }

int main(void)
{
    HAL_Init();
    //SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_SPI2_Init();
    MX_USART2_UART_Init();
    MX_USART1_UART_Init();
    dev_ctx.write_reg = (stmdev_write_ptr)platform_write;
    dev_ctx.read_reg = (stmdev_read_ptr)platform_read;
    dev_ctx.handle = &hspi1;
    HAL_Delay(20);

    /* Restore default configuration */
    lsm6ds3_reset_set(&dev_ctx, PROPERTY_ENABLE);

    {
        uint8_t rst;
        do {
            lsm6ds3_reset_get(&dev_ctx, &rst);
        } while (rst);
    }
    /*  Enable Block Data Update */
    lsm6ds3_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
    /* Set full scale */
    lsm6ds3_xl_full_scale_set(&dev_ctx, LSM6DS3_2g);
    lsm6ds3_gy_full_scale_set(&dev_ctx, LSM6DS3_250dps);
    /* Set Output Data Rate for Acc and Gyro */
    lsm6ds3_xl_data_rate_set(&dev_ctx, LSM6DS3_XL_ODR_6k66Hz);
    lsm6ds3_gy_data_rate_set(&dev_ctx, LSM6DS3_GY_ODR_1k66Hz);
    lsm6ds3_xl_power_mode_set(&dev_ctx, LSM6DS3_XL_HIGH_PERFORMANCE);
    lsm6ds3_gy_power_mode_set(&dev_ctx, LSM6DS3_GY_HIGH_PERFORMANCE);
    lsm6ds3_device_id_get(&dev_ctx, &chip_id);

    // Calibrate ADC at power up
    HAL_ADCEx_Calibration_Start(&hadc1);

    HAL_ADC_Start(&hadc1);

    HAL_Delay(1000);

    __enable_irq();

    while (1) {

    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 8;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA12 [PA10]     ------> ADC1_IN16
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.LowPowerAutoPowerOff = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_79CYCLES_5;
    hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_79CYCLES_5;
    hadc1.Init.OversamplingMode = DISABLE;
    hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_SPI2_Init(void)
{
    hspi1.Instance = SPI2;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    HAL_UART_Receive_IT(&huart1, &c1, 1);
}

static void MX_USART2_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }

    HAL_UART_Receive_IT(&huart2, &c2, 1);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, LED1_Pin | LED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, FIRE1_Pin | FIRE2_Pin | LED3_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = SIG_Pin_1;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SIG_1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SIG_Pin_2;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SIG_2_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = SIG_Pin_3;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SIG_3_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = SIG_Pin_4;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SIG_4_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = SIG_Pin_5;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SIG_5_GPIO_Port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

    GPIO_InitStruct.Pin = FIRE1_Pin | FIRE2_Pin | LED3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED1_Pin | LED2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pin : CS_Pin */
    GPIO_InitStruct.Pin = CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : IMU_INT_Pin */
    GPIO_InitStruct.Pin = IMU_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(IMU_INT_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : CAP_OUT_Pin */
    GPIO_InitStruct.Pin = CAP_OUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CAP_OUT_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : SHUNT_POS_Pin */
    GPIO_InitStruct.Pin = SHUNT_POS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SHUNT_POS_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : CAP_IN_Pin */
    GPIO_InitStruct.Pin = CAP_IN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CAP_IN_GPIO_Port, &GPIO_InitStruct);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        HAL_UART_Receive_IT(&huart1, &c1, 1);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		process_command(c1);

		/* Restart reception */
		HAL_UART_Receive_IT(&huart1, &c1, 1);
	}

	/* -------- USART2 INVERT ECHO TEST -------- */
	else if (huart->Instance == USART2) {
		uint8_t inverted = ~c2;

		HAL_UART_Transmit(&huart2, &inverted, 1, 100);

		HAL_UART_Receive_IT(&huart2, &c2, 1);
	}
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == SIG_Pin_1) {
    }
    if (GPIO_Pin == SIG_Pin_2) {
    }
    if (GPIO_Pin == SIG_Pin_3) {
    }
    if (GPIO_Pin == SIG_Pin_4) {
    }
    if (GPIO_Pin == SIG_Pin_5) {
    }
}

void Error_Handler(void)
{

    __disable_irq();
    while (1) {
    }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
