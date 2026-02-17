#include "main.h"
#include "esad.h"
#include "leds.h"
#include "lsm6ds3_reg.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"

//#define DEBUG

ADC_HandleTypeDef  hadc1;
SPI_HandleTypeDef  hspi1;
TIM_HandleTypeDef  htim16;
TIM_HandleTypeDef  htim17;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef *timer_seconds = &htim16;
TIM_HandleTypeDef *timer_20ms    = &htim17;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI2_Init(void);
#ifdef DEBUG
static void MX_USART2_UART_Init(void);
#endif
static void MX_USART1_UART_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM17_Init(void);
void adc_select_batt(void);
void adc_select_temp(void);
void detonate();
void undetonate();

volatile uint32_t metric_threshold;
static esad_msg_t in_cmd, out_cmd;
static stmdev_ctx_t dev_ctx;
static uint32_t metric_acc = 0;
static uint32_t metric_ang = 0;
static uint32_t metric_max_acc = 0;
static uint32_t metric_max_ang = 0;
static state_t state;

#define UART_BUFFER_SIZE 64 // Small buffer size for this example

typedef struct {
    uint8_t buffer[UART_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} CircularBuffer;

CircularBuffer uartBuffer;

void cb_init(CircularBuffer *cb);
int  cb_write(CircularBuffer *cb, uint8_t data);
int  cb_read(CircularBuffer *cb, uint8_t *data);
int  cb_count(CircularBuffer *cb);

void cb_init(CircularBuffer *cb)
{
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
}

int cb_write(CircularBuffer *cb, uint8_t data)
{
    if (cb->count < UART_BUFFER_SIZE) {
        cb->buffer[cb->head] = data;
        cb->head = (cb->head + 1) % UART_BUFFER_SIZE;
        cb->count++;
        return 1;  // Success
    }
    return 0;  // Buffer full
}

int cb_read(CircularBuffer *cb, uint8_t *data)
{
    if (cb->count > 0) {
        *data = cb->buffer[cb->tail];
        cb->tail = (cb->tail + 1) % UART_BUFFER_SIZE;
        cb->count--;
        return 1;  // Success
    }
    return 0;  // Buffer empty
}

int cb_count(CircularBuffer *cb)
{
    return cb->count;
}

uint16_t secs_to_armable()
{
    return params.ARMING_DELAY_SECS < state.uptime_secs
               ? 0
               : params.ARMING_DELAY_SECS - state.uptime_secs;
}

uint16_t secs_to_selfdestroy()
{
    return params.SELF_DESTRUCT_DELAY_SEC < state.secs_after_arm
               ? 0
               : params.SELF_DESTRUCT_DELAY_SEC - state.secs_after_arm;
}

void on_start()
{
    state.fired_latch = 0;
    state.arm_switch_on = 0;
    state.fire_switch_on = 0;
    state.fc_timeout = 0xFF;
    state.fc_version_ok = 0;
    state.uptime_secs = 0;
    state.secs_after_arm = 0;
}

void run_safe()
{
    undetonate();
    set_green(0);
    state.arm_switch_on = 0;
    state.fire_switch_on = 0;
    state.secs_after_arm = 0;
    out_cmd.command = safe;
    out_cmd.data = secs_to_armable();
}

void run_arm()
{
    if (secs_to_armable() != 0) {
        run_safe();
        return;
    }

    out_cmd.command = state.fired_latch ? fire : arm;
    out_cmd.data = default_data;
    state.arm_switch_on = 1;
    set_green(1);
}

void run_version()
{
    state.fc_version_ok = (in_cmd.data == sadcp_version);

    out_cmd.command = version;
    out_cmd.data = sadcp_version;
}

void run_fire()
{
    if (state.arm_switch_on) {
        state.fire_switch_on = 1;
        out_cmd.command = fire;
        return;
    } else {
        run_safe();
    }
}

uint8_t c;
void drop_one_byte() { HAL_UART_Receive_IT(&huart1, &c, 1); }

static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len)
{
    /* Write multiple command */
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &reg, 1, 1000);
    HAL_SPI_Transmit(handle, (uint8_t *)bufp, len, 1000);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    return 0;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
    reg |= 0x80;
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &reg, 1, 1000);
    HAL_SPI_Receive(handle, bufp, len, 1000);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

    return 0;
}

void undetonate()
{
    if (!state.fired_latch) return;
    state.fired_latch = 0;

    HAL_GPIO_WritePin(FIRE1_GPIO_Port, FIRE1_Pin, 0);
    HAL_GPIO_WritePin(FIRE2_GPIO_Port, FIRE2_Pin, 0);
    HAL_Delay(200);
}

void detonate()
{
    if (state.fired_latch) return;
    state.fired_latch = 1;

    HAL_GPIO_WritePin(FIRE1_GPIO_Port, FIRE1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FIRE2_GPIO_Port, FIRE2_Pin, GPIO_PIN_SET);
    HAL_Delay(200);
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

static bool battery_check(void)
{
    const uint16_t bat_ok_threshold = 8500;
    uint16_t mv = 0;
    HAL_GPIO_WritePin(BAT_TEST_GPIO_Port, BAT_TEST_Pin, GPIO_PIN_SET);
    adc_select_batt();
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    mv = adc_to_mv(HAL_ADC_GetValue(&hadc1));
    HAL_ADC_Stop(&hadc1);
    HAL_GPIO_WritePin(BAT_TEST_GPIO_Port, BAT_TEST_Pin, GPIO_PIN_RESET);

    return mv > bat_ok_threshold ? true:false;
}


#define VDDA_APPLI                     ((uint32_t) 3300)        /* Value of analog reference voltage (Vref+), connected to analog voltage supply Vdda (unit: mV) */
#define TEMPSENSOR_TYP_CAL1_V          (( int32_t)  760)        /* Internal temperature sensor, parameter V30 (unit: mV). Refer to device datasheet for min/typ/max values. */
#define TEMPSENSOR_TYP_AVGSLOPE        (( int32_t) 2500)        /* Internal temperature sensor, parameter Avg_Slope (unit: uV/DegCelsius). Refer to device datasheet for min/typ/max values. */
#define TEMPSENSOR_CAL_VREF            ((uint32_t) 3000)        /* Vdda value with which temperature sensor has been calibrated in production (+-10 mV). */
static uint16_t temp_check(void)
{
    adc_select_temp();
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    uint16_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    uint16_t temperature = __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(TEMPSENSOR_TYP_AVGSLOPE,
                                                        TEMPSENSOR_TYP_CAL1_V,
                                                        TEMPSENSOR_CAL1_TEMP,
                                                        VDDA_APPLI,
                                                        val,
                                                        LL_ADC_RESOLUTION_12B);
    return temperature;
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

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

void adc_select_batt(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_16;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

void adc_select_temp(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

static uint8_t output_flip = 0;
int main(void)
{
    uint8_t  reg;
    HAL_Init();
    SystemClock_Config();
    on_start();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_SPI2_Init();
    cb_init(&uartBuffer);
    #ifdef DEBUG
    MX_USART2_UART_Init();
    #endif
    MX_USART1_UART_Init();
    MX_TIM16_Init();
    MX_TIM17_Init();
    startup_flash();
    static i16xyz_t acceleration, angular_rate;
    dev_ctx.write_reg = platform_write;
    dev_ctx.read_reg = platform_read;
    dev_ctx.handle = &hspi1;
    HAL_Delay(params.BOOT_DELAY_MSEC);

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
    
    doubledouble_flash();

    // Calibrate ADC at power up
    HAL_ADCEx_Calibration_Start(&hadc1);
    
    HAL_TIM_Base_Start_IT(timer_20ms);
    HAL_UART_TxCpltCallback(&huart1);
    HAL_ADC_Start(&hadc1);

    HAL_Delay(1000);

    __enable_irq();

    while (1) {
        if (blasting_cap_check() == CAP_OK) {
            set_red(1);
        }

        if (blasting_cap_check() == CAP_NOT_CONNECTED) {
            set_red(0);
        }

        if (blasting_cap_check() == CAP_SHORTED) {
        	undetonate();
            set_red(blink_pattern(5, 0xAA));
            set_green(blink_pattern(5, 0xAA));
            set_yellow(blink_pattern(5, 0xAA));
        }

        lsm6ds3_xl_flag_data_ready_get(&dev_ctx, &reg);
        //lsm6ds3_xl_full_scale_set(&dev_ctx, LSM6DS3_16g);

        metric_threshold = params.METRIC_HIT_THRESHOLD;

        static i16xyz_t acc_prev;
        if (reg) {
            lsm6ds3_acceleration_raw_get(&dev_ctx, acceleration.mem);
            metric_acc = (abs(acceleration.x - acc_prev.x) +
                          abs(acceleration.y - acc_prev.y) +
                          abs(acceleration.z - acc_prev.z));
            if (metric_acc > metric_max_acc) metric_max_acc = metric_acc;

            static uint32_t abs_sum = 0;
            abs_sum =
                abs(acceleration.x) + abs(acceleration.y) + abs(acceleration.z);

            acc_prev = acceleration;

            // don't run timers until first sensor data arrives
            // to check if sensor gives data
            // and the data is not (0, 0, 0)
            if (HAL_TIM_Base_GetState(timer_seconds) == HAL_TIM_STATE_READY &&
                (abs_sum > 0)) {
                HAL_TIM_Base_Start_IT(timer_seconds);
            }
        }

        static i16xyz_t ang_prev;

        lsm6ds3_gy_flag_data_ready_get(&dev_ctx, &reg);
        if (reg) {
            lsm6ds3_angular_rate_raw_get(&dev_ctx, angular_rate.mem);

            metric_ang = (abs(angular_rate.x - ang_prev.x) +
                          abs(angular_rate.y - ang_prev.y) +
                          abs(angular_rate.z - ang_prev.z));
            if (metric_ang > metric_max_ang) metric_max_ang = metric_ang;
            #ifdef DEBUG
            sprintf((char *)uart_tx_buf, "SELF_DESTROY IN (SEC): %i\r\n", secs_to_selfdestroy());
            HAL_UART_Transmit_IT(&huart2, uart_tx_buf,
                                 strlen((char const *)uart_tx_buf));
            #endif
        } else {
            metric_ang = 0;
        }
        ang_prev = angular_rate;

        if (metric_ang > params.METRIC_HIT_THRESHOLD || metric_acc > params.METRIC_HIT_THRESHOLD) {
            if (output_flip & 0x01) detonate();
            else undetonate();
            output_flip ^= 1;
        }
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

static void MX_TIM16_Init(void)
{
    htim16.Instance = TIM16;
    htim16.Init.Prescaler = 8000 - 1;
    htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim16.Init.Period = 8000 - 1;
    htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim16.Init.RepetitionCounter = 0;
    htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim16) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_TIM17_Init(void)
{
    htim17.Instance = TIM17;
    htim17.Init.Prescaler = 16000 - 1;
    htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim17.Init.Period = 22 - 1;
    htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim17.Init.RepetitionCounter = 0;
    htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim17) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 57600;
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

    HAL_UART_Receive_IT(&huart1, uartBuffer.buffer, 1);
}


#ifdef DEBUG
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
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
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
}
#endif

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

    /*Configure GPIO pin : SIG_Pin */
    /*GPIO_InitStruct.Pin = SIG_Pin_1;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SIG_1_GPIO_Port, &GPIO_InitStruct);*/

    GPIO_InitStruct.Pin = SIG_Pin_2;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SIG_2_GPIO_Port, &GPIO_InitStruct);

	/*GPIO_InitStruct.Pin = SIG_Pin_3;
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
	HAL_GPIO_Init(SIG_5_GPIO_Port, &GPIO_InitStruct);*/

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

    /*Configure GPIO pin : BAT_TEST_Pin */
    GPIO_InitStruct.Pin = BAT_TEST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BAT_TEST_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BAT_TEST_GPIO_Port, BAT_TEST_Pin, 0);

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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    uint8_t c;
    if (htim == timer_seconds) {
        if (CAP_SHORTED != blasting_cap_check()) state.uptime_secs++;
        	else state.uptime_secs = 0;
        if (state.arm_switch_on) state.secs_after_arm++;
    } else if (htim == timer_20ms) {
    	if(!state.comm_timeout) set_yellow(0);
    	if (state.comm_timeout) state.comm_timeout--;
        if (cb_count(&uartBuffer) >= 4) {
            for (uint8_t i = 0; i < 4; i++) cb_read(&uartBuffer, &in_cmd.buf[i]);

            out_cmd.mem = 0x00;
            out_cmd.data = 0x00;

            if (esad_msg_checksum(&in_cmd) == 0x0) {
                switch (in_cmd.command) {
                    case safe:
                        run_safe();
                        break;
                    case fire:
                        run_fire();
                        break;
                    case arm:
                        run_arm();
                        break;
                    case no_radio:
                        break;
                    default: break;
                }
                state.comm_timeout = 5;
                if (state.comm_timeout) set_yellow(1);
                set_esad_msg_checksum(&out_cmd);
                HAL_UART_Transmit_IT(&huart1, out_cmd.buf, 4);
            } else {
                cb_read(&uartBuffer, &c);
            }
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    drop_one_byte();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    HAL_UART_Receive_IT(huart, in_cmd.buf, 4);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1) {
        cb_write(&uartBuffer, huart->Instance->RDR & 0xFF);  // Read received byte and write to buffer
        HAL_UART_Receive_IT(&huart1, uartBuffer.buffer, 1);  // Prepare for the next byte
    }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == SIG_Pin_2 && state.arm_switch_on) {
        //detonate();
    }
}

void Error_Handler(void)
{
    set_green(1);
    set_yellow(1);
    set_red(1);

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
