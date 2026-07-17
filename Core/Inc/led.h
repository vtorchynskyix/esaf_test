#ifndef LED_H
#define LED_H

#include "stm32g0xx.h"

class Led {
public:
    void init(GPIO_TypeDef* GPIOx, uint32_t pin);
    void on(GPIO_TypeDef* GPIOx, uint32_t pin);
    void off(GPIO_TypeDef* GPIOx, uint32_t pin);
    void toggle(GPIO_TypeDef* GPIOx, uint32_t pin);
    bool is_on(GPIO_TypeDef* GPIOx, uint32_t pin);
    bool is_off(GPIO_TypeDef* GPIOx, uint32_t pin);
    void blink(GPIO_TypeDef* GPIOx, uint32_t pin, uint32_t delay_ms);
    void blink_fast(GPIO_TypeDef* GPIOx, uint32_t pin);
    void blink_slow(GPIO_TypeDef* GPIOx, uint32_t pin);
    void blink_pattern(GPIO_TypeDef* GPIOx, uint32_t pin, const uint32_t* pattern, size_t length);
    void strobe(GPIO_TypeDef* GPIOx, uint32_t pin, uint32_t delay_ms);
};

#endif

