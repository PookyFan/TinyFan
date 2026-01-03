#ifndef __DISPLAY_H__
#define __DISPLAY_H__

//Display used is controlled with two chained HC595 shift registers, so that first HC595
//in chain controls which segment(s) to activate at the moment (with one or more bits set
//to high), and second HC595 in chain keeps actual BCD value to be shown.
//A single segment is structured as follows:
//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---     - <--- dot
//      D      H
//
//Segment can be then described as byte with bits ABCDEFGH (0 - line/dot lit, 1 - dimmed).
//This order results from how HC595s are wired to segments, and decision that they are fed data
//starting from least significant bit. This also maps most significant digit bit to leftmost segment.

#define _SEGA BIT(7)
#define _SEGB BIT(6)
#define _SEGC BIT(5)
#define _SEGD BIT(4)
#define _SEGE BIT(3)
#define _SEGF BIT(2)
#define _SEGG BIT(1)
#define _SEGH BIT(0)

#define NEG(n) (uint8_t)(~(n))

#define DISP_0 NEG(_SEGA | _SEGB | _SEGC | _SEGD | _SEGE | _SEGF)
#define DISP_1 NEG(_SEGB | _SEGC)
#define DISP_2 NEG(_SEGA | _SEGB | _SEGG | _SEGE | _SEGD)
#define DISP_3 NEG(_SEGA | _SEGB | _SEGC | _SEGD | _SEGG)
#define DISP_4 NEG(_SEGB | _SEGC | _SEGF | _SEGG)
#define DISP_5 NEG(_SEGA | _SEGF | _SEGG | _SEGC | _SEGD)
#define DISP_6 NEG(_SEGA | _SEGC | _SEGD | _SEGE | _SEGF | _SEGG)
#define DISP_7 NEG(_SEGA | _SEGB | _SEGC)
#define DISP_8 NEG(_SEGA | _SEGB | _SEGC | _SEGD | _SEGE | _SEGF | _SEGG)
#define DISP_9 NEG(_SEGA | _SEGB | _SEGC | _SEGD | _SEGF | _SEGG)
#define DISP_P NEG(_SEGA | _SEGB | _SEGG | _SEGF | _SEGE)
#define DISP_DOT NEG(_SEGH)
#define DISP_NONE 0xFF

#define SEGMENTS_COUNT 4
#define MAX_VALUE_TO_DISPLAY 9999
#define FIRST_BCD_NIBBLE 4
#define PAST_LAST_BCD_NIBBLE 8

#define RIGHTSHIFT_BY_NIBBLE(v, n) ((v) >> (4 * (n)))
#define LEFTSHIFT_BY_NIBBLE(v, n) ((v) << (4 * (n)))
#define NIBBLE_MASK(n) LEFTSHIFT_BY_NIBBLE(0xFUL, (n))
#define NIBBLE_OF(v, n) ((v) & NIBBLE_MASK(n))
#define BITS_IN(n) (8 * sizeof(n))

void setup_display();
void set_displayed_number(uint16_t number);
void display_digit(uint8_t digit);

#endif