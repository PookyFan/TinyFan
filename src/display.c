#include <stdint.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "display.h"
#include "ports.h"

static volatile uint8_t displayed_characters[SEGMENTS_COUNT] = {DISP_N, DISP_A, DISP_F, DISP_T};

static const uint8_t display_characters_map[] PROGMEM = {
    DISP_0, DISP_1, DISP_2, DISP_3, DISP_4, DISP_5, DISP_6, DISP_7, DISP_8, DISP_9,
    DISP_9 & DISP_DOT //last character is 9 with dot
};

static uint8_t get_display_character(uint8_t index)
{
    return pgm_read_byte(&display_characters_map[index]);
}

static uint16_t encode_bcd(uint16_t number)
{
    uint32_t reg = number;
    for(uint8_t l = 0; l < BITS_IN(number); ++l)
    {
        for(uint8_t i = FIRST_BCD_NIBBLE; i < PAST_LAST_BCD_NIBBLE; ++i)
        {
            if(NIBBLE_OF(reg, i) >= LEFTSHIFT_BY_NIBBLE(5UL, i))
                reg += LEFTSHIFT_BY_NIBBLE(3UL, i);
        }
        reg <<= 1;
    }

    return (uint16_t)(reg >> BITS_IN(number));
}

void setup_display()
{
    set_pin_out(DISP_DATA_PORT);
    set_pin_out(DISP_SCLK_PORT);
    set_pin_out(DISP_RCLK_PORT);
}

void set_displayed_number(uint16_t number)
{
    if(number > MAX_VALUE_TO_DISPLAY)
    {
        //If value is too large to be shown, encode special indices to show 9999 with dots
        const uint8_t nine_with_dot_index = sizeof(display_characters_map) - 1;
        number = nine_with_dot_index
            | LEFTSHIFT_BY_NIBBLE(nine_with_dot_index, 1)
            | LEFTSHIFT_BY_NIBBLE(nine_with_dot_index, 2)
            | LEFTSHIFT_BY_NIBBLE(nine_with_dot_index, 3);
    }
    else
        number = encode_bcd(number);
    
    uint8_t first_digit_encoded = 0;
    uint8_t characters[SEGMENTS_COUNT] = {DISP_NONE, DISP_NONE, DISP_NONE, DISP_NONE};
    for(uint8_t i = SEGMENTS_COUNT; i > 0; --i)
    {
        uint8_t digit_order = i - 1;
        uint8_t index = RIGHTSHIFT_BY_NIBBLE(number, digit_order) & 0xF;

        //If 0 is to be shown, make sure it's not a left-most character
        if(index > 0 || first_digit_encoded)
        {
            characters[digit_order] = get_display_character(index);
            first_digit_encoded = 1;
        }
    }

    if(!first_digit_encoded) //A single 0 is to be shown
        characters[0] = DISP_0;

    for(uint8_t i = 0; i < SEGMENTS_COUNT; ++i)
        displayed_characters[i] = characters[i];

}

void display_digit(uint8_t digit)
{
    //Shift BCD and segment mask to HC595s
    uint16_t value = (0x8000 >> digit) | displayed_characters[digit];
    for(uint8_t i = 0; i < BITS_IN(value); ++i)
    {
        set_pin(DISP_DATA_PORT, value & 1);
        set_pin_high(DISP_SCLK_PORT);
        value >>= 1;
        set_pin_low(DISP_SCLK_PORT);
    }

    //Latch shift registers values and feed them to display controler
    set_pin_high(DISP_RCLK_PORT);
    _NOP();
    set_pin_low(DISP_RCLK_PORT);
}