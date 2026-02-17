#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g0xx_hal.h"

typedef struct {
    union {
        struct {
            int16_t x, y, z;
        };
        int16_t mem[3];
    };
} i16xyz_t;

void Error_Handler(void);

#define SIG_Pin_1 GPIO_PIN_1
#define SIG_1_GPIO_Port GPIOB

#define SIG_Pin_2 GPIO_PIN_2
#define SIG_2_GPIO_Port GPIOB

#define SIG_Pin_3 GPIO_PIN_8
#define SIG_3_GPIO_Port GPIOA

#define SIG_Pin_4 GPIO_PIN_5
#define SIG_4_GPIO_Port GPIOA

#define SIG_Pin_5 GPIO_PIN_11
#define SIG_5_GPIO_Port GPIOA

#define FIRE1_Pin GPIO_PIN_3
#define FIRE1_GPIO_Port GPIOB

#define FIRE2_Pin GPIO_PIN_4
#define FIRE2_GPIO_Port GPIOB

#define LED1_Pin GPIO_PIN_6
#define LED1_GPIO_Port GPIOA

#define LED2_Pin GPIO_PIN_7
#define LED2_GPIO_Port GPIOA

#define LED3_Pin GPIO_PIN_0
#define LED3_GPIO_Port GPIOB

#define V_MON_ADC_Pin GPIO_PIN_12  //(PA10???)
#define V_MON_ADC_GPIO_Port GPIOA

#define CS_Pin GPIO_PIN_9
#define CS_GPIO_Port GPIOB

#define IMU_INT_Pin GPIO_PIN_5
#define IMU_INT_GPIO_Port GPIOB

#define CAP_IN_Pin GPIO_PIN_0
#define CAP_IN_GPIO_Port GPIOA

#define CAP_OUT_Pin GPIO_PIN_14
#define CAP_OUT_GPIO_Port GPIOC

#define SHUNT_POS_Pin GPIO_PIN_6
#define SHUNT_POS_GPIO_Port GPIOC

#define BAT_TEST_Pin GPIO_PIN_15
#define BAT_TEST_GPIO_Port GPIOA

typedef struct {
    uint16_t BOOT_DELAY_MSEC;
    uint16_t ARMING_DELAY_SECS;
    uint16_t SELF_DESTRUCT_DELAY_SEC;
    uint32_t METRIC_HIT_THRESHOLD;
    uint32_t METRIC_TRAP_ACC_THRESHOLD;
    uint32_t METRIC_TRAP_ANG_THRESHOLD;

} params_t;

const static params_t params = {
    // small delay to boot the sensor
    .BOOT_DELAY_MSEC = 20,

    // arming delay counter in seconds to safely launch the drone, default = 120
    .ARMING_DELAY_SECS = 5 * 60,
    .SELF_DESTRUCT_DELAY_SEC = 60 * 60,
    .METRIC_HIT_THRESHOLD = 150 * 400,
    .METRIC_TRAP_ACC_THRESHOLD = 256 * 400,
    .METRIC_TRAP_ANG_THRESHOLD = 256 * 400
};

// detonate if lost power after 600*1000 milliseconds and voltage is low
#define POWER_LOSS_DESTRUCT_DELAY 6 * 1000
#define VMON_THRESHOLD 1500

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
