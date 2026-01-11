
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>
#include "display.h"
#include "ports.h"

#define TIMER_FREQ   50000
#define AS_NUMBER    0
#define AS_PERCENT   1
#define ADC_LOW_VAL  147
#define ADC_HIGH_VAL 247

static volatile struct {
    uint16_t display_delay;
    uint16_t fan_revolution_pulses;
    uint16_t fan_revolution_count;
    uint8_t  periph_delay;
    uint8_t  prev_adc_value;
    uint8_t  curr_adc_value;
    uint8_t : 4;
    uint8_t  update_fan_speed : 1;
    uint8_t  display_percentage : 1;
    uint8_t  current_character : 2;
} global;

ISR(INT0_vect)
{
    ++global.fan_revolution_pulses;
}

ISR(TIM0_COMPA_vect)
{
    //todo: toggle PWM pin to achieve 25 kHz PWM signal from 50 kHz timer

    if(++global.display_delay == TIMER_FREQ)
    {
        global.display_delay = 0;
        global.fan_revolution_count = global.fan_revolution_pulses >> 1;
        global.fan_revolution_pulses = global.fan_revolution_pulses & 1;
        global.update_fan_speed = 1;
        global.display_percentage = 0;
    }

    if(++global.periph_delay == 208) //For 50 kHz main timer we'll get ~240 Hz peripheral timer
    {
        display_character(++global.current_character);
        set_bit(ADCSRA, ADSC); //Start new ADC conversion while no communication with display is taking place
        global.periph_delay = 0;
    }
}

ISR(ADC_vect)
{
    uint8_t result = ADCH; //Ignore LSB of the result
    global.prev_adc_value = global.curr_adc_value;
    global.curr_adc_value = result;
}

int main()
{
    cli();

    //Init display
    setup_display();

    //Setup ADC on channel 0
    DIDR0 = BIT(ADC0D); //Disable digital input on ADC0 pin
    ADMUX = BIT(ADLAR); //Adjust ADC results left (we'll then be using only high word of conversion result)
    ADCSRA = BIT(ADEN) | BIT(ADSC) | BIT(ADIE); //Enable and start ADC, and enable ADC interrupt

    //Set fan revolution sensor interrupt and enable sleep mode
    MCUCR = BIT(PUD) | BIT(SE) | BIT(ISC01); //INT0 on falling edge, also disable pull-ups and enable sleep
    GIMSK = BIT(INT0); //Enable INT0 interrupt

    //Init 50 kHz timer for 25 kHz software PWM mode
    TIMSK0 = BIT(OCIE0A); //Enable interrupt for Compare Match A
    OCR0A = 95; //Needed to achieve 50 kHz with no prescaler at 4.8 MHz
    TCCR0A = BIT(WGM01); //CTC mode
    TCCR0B = BIT(CS00); //Prescaler = 1 (no prescaler)

    //All initialization done
    sei();

    //Wait for a while to show banner on display before starting normal operation
    for(uint8_t i = 1; i > 0; --i)
        _delay_loop_2(60000);

    while(1)
    {
        sleep_cpu();
        cli();
        uint8_t current = global.curr_adc_value;
        uint8_t previous = global.prev_adc_value;
        sei();
        if(current != previous)
        {
            cli();
            global.display_delay = 0;
            global.display_percentage = 1;
            global.fan_revolution_pulses = 0;
            sei();

            uint8_t percent = (current > ADC_HIGH_VAL) ? 100 :
                ((current < ADC_LOW_VAL) ? 0 : current - ADC_LOW_VAL);
            set_displayed_number(percent, AS_PERCENT);
        }
        else if(!global.display_percentage && global.update_fan_speed)
        {
            cli();
            uint16_t revolutions = global.fan_revolution_count;
            global.update_fan_speed = 0;
            sei();
            set_displayed_number(revolutions, AS_NUMBER);
        }
    }
    return 0;
}