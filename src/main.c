#include <avr/cpufunc.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <util/delay_basic.h>
#include "display.h"
#include "ports.h"
#include "register_structs.h"

#define HIGH         1
#define LOW          0
#define TIMER_FREQ   25000
#define AS_NUMBER    0
#define AS_PERCENT   1
#define ADC_LOW_VAL  147U
#define ADC_HIGH_VAL 247U

static volatile struct {
    uint16_t fan_revolution_count;
    uint8_t  current_character;
} global_ram;

static const uint8_t precalculated_timer_b_values[] PROGMEM = {
    /*0-4% not handled*/ 9, 11, 13, 15, 17, 19, 21, 22, 24, 26, 28, 30, 32, 34, 36, 38,
    40, 42, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 64, 66, 68, 70, 72, 74, 76,
    78, 80, 82, 84, 85, 87, 89, 91, 93, 95, 97, 99, 101, 103, 105, 106, 108, 110, 112, 114,
    116, 118, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 148, 150, 152,
    154, 156, 158, 160, 162, 164, 166, 168, 169, 171, 173, 175, 177, 179, 181, /*96-100% not handled*/
};

static const uint8_t EEMEM calibration_value_ee = CALIBRATION_VALUE;

uint8_t get_precalculated_timer_b(unsigned int index)
{
    return pgm_read_byte(&precalculated_timer_b_values[index - 5]);
}

static void disable_pwm_and_set_pin(uint8_t value)
{
    set_pin(FAN_PWM_PORT, value);
    global_reg.flags.pwm_enabled = 0;
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
    if(global_reg.flags.pwm_enabled)
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
        global_reg.flags.update_fan_speed = 1;
        global_reg.flags.display_percentage = 0;
        sei();
    }

    if(++global_reg.periph_delay == 104) //For 25 kHz main timer we'll get ~240 Hz peripheral timer
    {
        global_reg.periph_delay = 0;
        global_reg.flags.handle_peripherals = 1;
    }
}

ISR(TIM0_COMPB_vect)
{
    if(global_reg.flags.pwm_enabled)
        set_pin_low(FAN_PWM_PORT);
}

ISR(ADC_vect)
{
    readings_reg.prev_adc_value = readings_reg.curr_adc_value;
    readings_reg.curr_adc_value = ADCH; //Ignore LSB of the result
}

inline static void display_next_character()
{
    global_ram.current_character = (global_ram.current_character + 1) & 3;
    display_character(global_ram.current_character);
}

inline static void initialize()
{
    cli();

    //Read calibration value from EEPROM
    EEARL = &calibration_value_ee;          //Load address in EEPROM to read from
    EECR = BIT(EERE);                      //Read from EEPROM
    const uint8_t calibration_val = EEDR; //Get read value from EEPROM

    //Calibrate internal oscilator
    while(OSCCAL != calibration_val)
    {
        int8_t diff = (OSCCAL < calibration_val) ? 1 : -1;
        OSCCAL += diff;
        _NOP();
    }

    //Init data kept in registers
    readings_reg.fan_revolution_pulses = 0;
    global_reg.display_delay = 0;
    global_reg.periph_delay = 0;
    global_reg.flags_reg = 0;

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
}

inline static void main_loop()
{
    uint8_t current = readings_reg.curr_adc_value;
    uint8_t previous = readings_reg.prev_adc_value;
    if(current != previous)
    {
        global_reg.flags.adc_changed = 1;
        global_reg.flags.adc_is_stable = 0;
    }
    else if(global_reg.flags.adc_changed && !global_reg.flags.adc_is_stable)
    {
        global_reg.flags.adc_changed = 0;
        global_reg.flags.adc_is_stable = 1;

        //PWM is somehow precise, although not so much close to edge values,
        //probably due to ISRs delays taking considerable time of level change
        uint8_t percent = current - ADC_LOW_VAL;
        uint8_t timer_val = 0xFF; //For 0 and 100 percent cases
        cli();
        if(percent < 5)
        {
            percent = 0;
            disable_pwm_and_set_pin(LOW);
        }
        else if(percent > 95)
        {
            percent = 100;
            disable_pwm_and_set_pin(HIGH);
        }
        else
        {
            timer_val = get_precalculated_timer_b(percent);
            global_reg.flags.pwm_enabled = 1;
        }

        if(timer_val == OCR0B)
        {
            //Timer value is the same, so no real change in fan speed occurs - skip the update
            sei();
            goto disp_char;
        }

        OCR0B = timer_val;
        global_reg.display_delay = 0;
        global_reg.flags.display_percentage = 1;
        readings_reg.fan_revolution_pulses = 0;
        sei();

        set_displayed_number(percent, AS_PERCENT);
    }
    else if(!global_reg.flags.display_percentage && global_reg.flags.update_fan_speed)
    {
        cli();
        uint16_t revolutions = global_ram.fan_revolution_count;
        global_reg.flags.update_fan_speed = 0;
        sei();

        //To get revolutions per minute instead of per second,
        //multiply by 64 then subtract original value times four,
        //as simply multiplying by 60 is a no-go on AVR
        uint16_t rpm = revolutions << 6;
        revolutions <<= 2;
        rpm -= revolutions;
        set_displayed_number(rpm, AS_NUMBER);
    }

disp_char:
    if(global_reg.flags.handle_peripherals)
    {
        global_reg.flags.handle_peripherals = 0;
        display_next_character();
        set_bit(ADCSRA, ADSC); //Start new ADC conversion while no communication with display is taking place
    }
}

int main()
{
    initialize();

    //Wait for a while to show banner on display before starting normal operation
    do
    {
        display_next_character();
        sleep_cpu();
    } while(!global_reg.flags.update_fan_speed);

    //Enforce speed percentage event change at the start
    cli();
    readings_reg.prev_adc_value = readings_reg.curr_adc_value;
    global_reg.flags.adc_changed = 1;

    while(1)
        main_loop();

    return 0;
}