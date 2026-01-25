
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>
#include "display.h"
#include "ports.h"
#include "register_structs.h"

#define HIGH         1
#define LOW          0
#define TIMER_FREQ   25000
#define AS_NUMBER    0
#define AS_PERCENT   1
#define ADC_LOW_VAL  147L
#define ADC_HIGH_VAL 247L

static volatile struct {
    uint16_t fan_revolution_count;
    uint8_t  current_character;
} global_ram;

static void disable_pwm_and_set_pin(uint8_t value)
{
    OCR0B = 0xFF; //Do not allow further compare match B events
    set_pin(FAN_PWM_PORT, value);
    global_reg.pwm_enabled = 0;
}

ISR(INT0_vect, ISR_NAKED)
{
    //readings_reg.fan_revolution_pulses += 1;
    asm volatile(
        "push r0" "\n\t"           //save temp register
        "in r0, 0x3f" "\n\t"      //save flags register
        "inc r8" "\n\t"          //add 1 to LSB
        "brne after_inc" "\n\t" //if no overflow in LSB, skip incrementing MSB
        "inc r9" "\n"          //add 1 to MSB
        "after_inc:" "\n\t"   //
        "out 0x3f, r0" "\n\t"//restore flags register
        "pop r0" "\n\t"     //restore temp register
        "reti"
    );
}

//We want compiler to generate pro- and epilogue for this helper handler, but not for main interrupt handler,
//so we need to treat helper handler like a real one - this also will add reti instruction at the end of it,
//and make sure it starts with sei instruction to enable nested interrupts (mainly for timer compare B)
void __vector_timer_cmp_match_A_handler() __attribute__((__used__, __interrupt__));

ISR(TIM0_COMPA_vect, ISR_NAKED)
{
    if(global_reg.pwm_enabled)
        set_pin_high(FAN_PWM_PORT);

    //No call to __vector_timer_cmp_match_A_handler() here - we will fall to it due to no reti instruction!
}

void __vector_timer_cmp_match_A_handler()
{
    if(++global_reg.display_delay == TIMER_FREQ)
    {
        global_reg.display_delay = 0;
        cli();
        global_ram.fan_revolution_count = readings_reg.fan_revolution_pulses >> 1;
        readings_reg.fan_revolution_pulses = readings_reg.fan_revolution_pulses & 1;
        global_reg.update_fan_speed = 1;
        global_reg.display_percentage = 0;
        sei();
    }

    if(++global_reg.periph_delay == 104) //For 25 kHz main timer we'll get ~240 Hz peripheral timer
    {
        global_ram.current_character = (global_ram.current_character + 1) & 3;
        display_character(global_ram.current_character);
        global_reg.periph_delay = 0;
        set_bit(ADCSRA, ADSC); //Start new ADC conversion while no communication with display is taking place
    }
}

ISR(TIM0_COMPB_vect)
{
    if(global_reg.pwm_enabled)
        set_pin_low(FAN_PWM_PORT);
}

ISR(ADC_vect)
{
    readings_reg.prev_adc_value = readings_reg.curr_adc_value;
    readings_reg.curr_adc_value = ADCH; //Ignore LSB of the result
}

int main()
{
    cli();

    //Init data kept in registers
    readings_reg.fan_revolution_pulses = 0;
    readings_reg.prev_adc_value = 0;
    readings_reg.curr_adc_value = 0;

    global_reg.display_delay = 0;
    global_reg.periph_delay = 0;
    global_reg.pwm_enabled = 0;
    global_reg.update_fan_speed = 0;
    global_reg.display_percentage = 0;

    //Init display
    set_pin_out(DISP_DATA_PORT);
    set_pin_out(DISP_SCLK_PORT);
    set_pin_out(DISP_RCLK_PORT);

    //Setup ADC on channel 0
    DIDR0 = BIT(ADC0D); //Disable digital input on ADC0 pin
    ADMUX = BIT(ADLAR); //Adjust ADC results left (we'll then be using only high word of conversion result)
    ADCSRA = BIT(ADEN) | BIT(ADSC) | BIT(ADIE); //Enable and start ADC, and enable ADC interrupt

    //Set fan revolution sensor interrupt and enable sleep mode
    MCUCR = BIT(PUD) | BIT(SE) | BIT(ISC01); //INT0 on falling edge, also disable pull-ups and enable sleep
    GIMSK = BIT(INT0); //Enable INT0 interrupt

    //Init 25 kHz timer for software PWM mode
    TIMSK0 = BIT(OCIE0A) | BIT(OCIE0B); //Enable interrupt for Compare Match A and B
    OCR0A = 191; //Needed to achieve 25 kHz with no prescaler at 4.8 MHz
    TCCR0A = BIT(WGM01); //CTC mode
    TCCR0B = BIT(CS00); //Prescaler = 1 (no prescaler)
    set_pin_out(FAN_PWM_PORT);

    //All initialization done
    sei();

    //Wait for a while to show banner on display before starting normal operation
    do
    {
        sleep_cpu();
    } while(!global_reg.update_fan_speed);
        

    //Enforce speed percentage event change at the start
    cli();
    readings_reg.prev_adc_value = 0;

    //Enter program loop
    while(1)
    {
        uint8_t current = readings_reg.curr_adc_value;
        uint8_t previous = readings_reg.prev_adc_value;
        if(current != previous)
        {
            cli();
            int16_t percent = ((int16_t)current) - ADC_LOW_VAL;
            uint8_t timer_b_val = ((uint8_t)percent) << 1;
            if(percent <= 0)
            {
                percent = 0;
                disable_pwm_and_set_pin(LOW);
            }
            else if(timer_b_val > OCR0A)
            {
                percent = 100;
                disable_pwm_and_set_pin(HIGH);
            }
            else
            {
                if(!global_reg.pwm_enabled)
                {
                    global_reg.pwm_enabled = 1;

                    //Clear fan counters and timer counter to properly align toggling PWM pin
                    readings_reg.fan_revolution_pulses = 0;
                    global_ram.fan_revolution_count = 0;
                }
                OCR0B = timer_b_val;
            }

            global_reg.display_delay = 0;
            global_reg.display_percentage = 1;
            readings_reg.fan_revolution_pulses = 0;
            sei();

            set_displayed_number(percent, AS_PERCENT);
        }
        else if(!global_reg.display_percentage && global_reg.update_fan_speed)
        {
            cli();
            uint16_t revolutions = global_ram.fan_revolution_count;
            global_reg.update_fan_speed = 0;
            sei();
            set_displayed_number(revolutions, AS_NUMBER);
        }

        sleep_cpu();
    }
    return 0;
}