#include "leds.h"
#include "main.h"

const uint16_t RED_LED    = LED1_Pin;
const uint16_t YELLOW_LED = LED2_Pin;
const uint16_t GREEN_LED  = LED3_Pin;

void set_green(int8_t val) { HAL_GPIO_WritePin(LED3_GPIO_Port, GREEN_LED, val); }

void blink_green(int8_t shift)
{
    int8_t val = (HAL_GetTick() >> shift) % 2;
    set_green(val);
}

void toggle_green()
{
    static int8_t val = 0;
    val = !val;
    HAL_GPIO_WritePin(LED3_GPIO_Port, GREEN_LED, val);
}

void set_yellow(int8_t val) { HAL_GPIO_WritePin(LED2_GPIO_Port, YELLOW_LED, val); }

void blink_yellow(int8_t shift)
{
    int8_t val = (HAL_GetTick() >> shift) % 2;
    set_yellow(val);
}

void toggle_yellow()
{
    static int8_t val = 0;
    val = !val;
    HAL_GPIO_WritePin(LED2_GPIO_Port, YELLOW_LED, val);
}

void set_red(int8_t val) { HAL_GPIO_WritePin(LED1_GPIO_Port, RED_LED, val); }

void blink_red(int8_t shift)
{
    int8_t val = (HAL_GetTick() >> shift) % 2;
    set_red(val);
}

void toggle_red()
{
    static int8_t val = 0;
    val = !val;
    HAL_GPIO_WritePin(LED1_GPIO_Port, RED_LED, val);
}

uint8_t blink_pattern(uint8_t prescale, uint8_t pattern)
{
    uint32_t tick = HAL_GetTick();
    uint32_t scaled_tick = tick >> prescale;
    uint32_t flash_is_hot = ((tick >> (prescale - 2)) & 0x3) == 0;
    if (!flash_is_hot) return 0;

    volatile uint8_t whichbit = scaled_tick % 8;
    volatile uint8_t bit_is_hot = (pattern >> whichbit) & 0x01;

    return (scaled_tick /*&& flash_is_hot*/ && bit_is_hot) > 0;
}

void startup_flash()
{
    int8_t j = 0;
    for (j = 0; j < 20; ++j) {
        // show both LED at startup
        set_green(j % 2);
        set_yellow((j + 1) % 2);
        set_red((j + 2) % 2);
        HAL_Delay(50);
    }
    set_green(0);
    set_yellow(0);
    set_red(0);
}

void doubledouble_flash()
{
    int8_t j = 0;
    for (j = 0; j <= 8; ++j) {
        // show both LED at startup
        set_green(j % 2);
        set_yellow(j % 2);
        set_red(j % 2);
        HAL_Delay(75);
    }
    set_green(0);
    set_yellow(0);
    set_red(0);
}
