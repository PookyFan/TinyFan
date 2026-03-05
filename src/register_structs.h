#ifndef __GLOBAL_REG_H__
#define __GLOBAL_REG_H__

#include <stdint.h>

register volatile struct {
    uint16_t fan_revolution_pulses; //r8-r9
    uint8_t  prev_adc_value;        //r10
    uint8_t  curr_adc_value;        //r11
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
            uint8_t  display_character  : 1; //r15.3
        } flags;
        uint8_t flags_reg; //r15
    };
} global_reg asm("r12");

#endif