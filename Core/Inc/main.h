#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g0xx_hal.h"

void Error_Handler(void);

#define SIG_Pin_1 GPIO_PIN_2
#define SIG_1_GPIO_Port GPIOB

#define SIG_Pin_2 GPIO_PIN_1
#define SIG_2_GPIO_Port GPIOB

#define SIG_Pin_3 GPIO_PIN_5
#define SIG_3_GPIO_Port GPIOA

#define SIG_Pin_4 GPIO_PIN_8
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

#define V_MON_ADC_Pin GPIO_PIN_12
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


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
