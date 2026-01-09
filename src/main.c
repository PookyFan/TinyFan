
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>
#include "display.h"

static volatile uint16_t fan_revelation_pulses;

ISR(INT0_vect)
{
    ++fan_revelation_pulses;
}

static volatile uint8_t current_digit;
static volatile uint8_t periph_delay;


ISR(TIM0_COMPA_vect)
{
    if(++periph_delay == 104) //For 25 kHz main timer we'll get ~240 Hz peripheral timer
    {
        current_digit = (current_digit + 1) & 0x3;
        display_digit(current_digit);
        ADCSRA |= BIT(ADSC); //Start new ADC conversion while no communication with display is taking place
        periph_delay = 0;
    }
}

static volatile uint8_t prev_adc_value;
static volatile uint8_t curr_adc_value;

ISR(ADC_vect)
{
    uint8_t result = ADCH; //Ignore LSB of the result
    prev_adc_value = curr_adc_value;
    curr_adc_value = result;
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

    //Set fan revelation sensor interrupt and enable sleep mode
    MCUCR = BIT(PUD) | BIT(SE) | BIT(ISC01) | BIT(ISC00); //INT0 on rising edge, also disable pull-ups and enable sleep

    //Init timer for 25 kHz software PWM mode
    TIMSK0 = BIT(OCIE0A); //Enable interrupt for Compare Match A
    OCR0A = 95; //Needed to achieve 25 kHz with no prescaler
    TCCR0A = BIT(WGM01); //CTC mode
    TCCR0B = BIT(CS00); //Prescaler = 1 (no prescaler)

    //All initialization done
    sei();

    //Wait for a while to show banner on display before starting normal operation
    for(uint8_t i = 5; i > 0; --i)
        _delay_loop_2(60000);

    set_displayed_number(curr_adc_value);
    while(1)
    {
        sleep_cpu();
        cli();
        uint8_t current = curr_adc_value;
        uint8_t previous = prev_adc_value;
        sei();
        if(current != previous)
            set_displayed_number(current);
    }
    return 0;
}