#include "../Inc/led.h"

#include "stm32g0xx.h"
#include "stm32g0xx_hal_tim.h"

#ifdef __cplusplus
 extern "C" {
#endif
class Led {
    Led() {
        init_timer(); // Initialize the timer for delay functions
    }

    ~Led() {
        // Cleanup if necessary
    }
private:
    GPIO_TypeDef* gpio_port;
    uint32_t gpio_pin;
    bool initialized = false;

    void init_timer() {
        // Initialize a timer for delay functions
        if (initialized) return; // Avoid re-initialization
        RCC->APBENR1 |= RCC_APBENR1_TIM17EN; // Enable TIM17 clock
        TIM17->PSC = 16000 - 1; // Prescaler for 1ms tick (assuming 16MHz clock)
        TIM17->ARR = 0xFFFFFFFF; // Max auto-reload value
        TIM17->CR1 |= TIM_CR1_CEN; // Start the timer
        initialized = true;
    }

    void delay_ms(uint32_t ms) {
        uint32_t start = TIM17->CNT;
        while ((TIM17->CNT - start) < ms);
    }

public:
    void init(GPIO_TypeDef* GPIOx, uint32_t pin) {
        // Enable the GPIO clock
        if (GPIOx == GPIOA) {
            RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
        } else if (GPIOx == GPIOB) {
            RCC->IOPENR |= RCC_IOPENR_GPIOBEN;
        } else if (GPIOx == GPIOC) {
            RCC->IOPENR |= RCC_IOPENR_GPIOCEN;
        }

        // Configure the pin as output
        uint32_t pin_pos = 0;
        while ((pin >> pin_pos) > 1) {
            pin_pos++;
        }
        GPIOx->MODER &= ~(0x3 << (pin_pos * 2)); // Clear mode bits
        GPIOx->MODER |= (0x1 << (pin_pos * 2));  // Set as output
    }

    void on(GPIO_TypeDef* GPIOx, uint32_t pin) {
        GPIOx->BSRR = pin; // Set the pin
    }

    void off(GPIO_TypeDef* GPIOx, uint32_t pin) {
        GPIOx->BRR = pin; // Reset the pin
    }

    void toggle(GPIO_TypeDef* GPIOx, uint32_t pin) {
        GPIOx->ODR ^= pin; // Toggle the pin
    }

    bool is_on(GPIO_TypeDef* GPIOx, uint32_t pin) {
        return (GPIOx->ODR & pin) != 0; // Check if the pin is set
    }

    bool is_off(GPIO_TypeDef* GPIOx, uint32_t pin) {
        return (GPIOx->ODR & pin) == 0; // Check if the pin is reset
    }

    void blink(GPIO_TypeDef* GPIOx, uint32_t pin, uint32_t delay_ms) {
        on(GPIOx, pin);
        delay_ms(delay_ms);
        off(GPIOx, pin);
    }

    void blink_fast(GPIO_TypeDef* GPIOx, uint32_t pin) {
        on(GPIOx, pin);
        delay_ms(100); // Fast delay
        off(GPIOx, pin);
    }

    void blink_slow(GPIO_TypeDef* GPIOx, uint32_t pin) {
        on(GPIOx, pin);
        delay_ms(1000); // Slow delay
        off(GPIOx, pin);
    }

    void blink_pattern(GPIO_TypeDef* GPIOx, uint32_t pin, const uint32_t* pattern, size_t length) {
        for (size_t i = 0; i < length; i++) {
            if (pattern[i] == 1) {
                on(GPIOx, pin);
            } else {
                off(GPIOx, pin);
            }
            delay_ms(100); // Delay between pattern steps
        }
    }

    void strobe(GPIO_TypeDef* GPIOx, uint32_t pin, uint32_t delay_ms) {
        for (int i = 0; i < 10; i++) { // Strobe for a short duration
            toggle(GPIOx, pin);
            delay_ms(delay_ms); // Delay between toggles
        }
    }

};
#ifdef __cplusplus
}
#endif
