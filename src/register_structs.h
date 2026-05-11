#ifndef __GLOBAL_REG_H__
#define __GLOBAL_REG_H__

#include <stdint.h>

register volatile struct {
    uint8_t fan_rev_pulses_current; //r8
    uint8_t  prev_adc_value;        //r9
    uint8_t  curr_adc_value;        //r10
    uint8_t  tmp;                   //r11
} readings_reg asm("r8");

register volatile struct {
    uint16_t display_delay; //r12-r13
    uint8_t  periph_delay;  //r14
    union                            
    {
        struct
        {
            uint8_t  pwm_enabled        : 1; //r15.0
            uint8_t  update_fan_speed   : 1; //r15.1
            uint8_t  display_percentage : 1; //r15.2
            uint8_t  handle_peripherals : 1; //r15.3
            uint8_t  adc_changed        : 1; //r15.4
            uint8_t  adc_is_stable      : 1; //r15.5
        } flags;
        uint8_t flags_reg; //r15
    };
} global_reg asm("r12");

register volatile struct {
    uint8_t  filter_val; //r16
    uint8_t filter_sum;  //r17
} lpf_reg asm("r16");

#endif