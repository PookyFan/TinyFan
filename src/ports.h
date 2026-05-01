#ifndef __PORTS_ASSIGNMENTS_H__
#define __PORTS_ASSIGNMENTS_H__

#include <avr/io.h>

#define FAN_PWM_PORT   0
#define FAN_SENSOR_PIN 1
#define DISP_DATA_PORT 4
#define DISP_RCLK_PORT 3
#define DISP_SCLK_PORT 2
#define KNOB_ADC_PIN   5

#define BIT(n) (1 << (n))

#define set_bit(r, n) (r |= BIT(n))
#define clear_bit(r, n) (r &= (uint8_t)(~BIT(n)))

#define set_pin_out(p) set_bit(DDRB, p)
#define set_pin_in(p) clear_bit(DDRB, p)

#define set_pin_high(p) set_bit(PORTB, p)
#define set_pin_low(p) clear_bit(PORTB, p)

#define set_pin(p, v) ((v) ? set_pin_high(p) : set_pin_low(p))

#endif