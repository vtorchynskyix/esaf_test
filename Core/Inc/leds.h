#ifndef LEDS_H_
#define LEDS_H_

#include "main.h"
#include <stdint.h>

void set_red(int8_t val);
void blink_red(int8_t shift);
void toggle_red();
void set_yellow(int8_t val);
void blink_yellow(int8_t shift);
void toggle_yellow();
void set_green(int8_t val);
void blink_green(int8_t shift);
void toggle_green();

uint8_t blink_pattern(uint8_t prescale, uint8_t pattern);

void startup_flash();
void doubledouble_flash();

#endif //LEDS_H_
