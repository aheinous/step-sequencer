
#include "config.h"
#include <Arduino.h>

const uint8_t CLK_OUT = 1<<4;
volatile uint8_t & CLK_OUT_WRITE = PORTD;
volatile uint8_t & CLK_OUT_DDR = DDRD;

const uint8_t POT_nENABLE = 1<<5;
const uint8_t LED_nENABLE = 1<<4;
const uint8_t MUX_ADDR = 0xF;
volatile uint8_t & MUX_WRITE = PORTB;
volatile uint8_t & MUX_DDR = DDRB;


const uint8_t nTAP = 1<<3;
volatile uint8_t & nTAP_WRITE = PORTD;
volatile uint8_t & nTAP_READ = PIND;
volatile uint8_t & nTAP_DDR = DDRD;

const uint8_t IOEX_INT = 1<<2;
volatile uint8_t & IOEX_INT_READ = PIND;

uint8_t RATE_ADC_CHANNEL = 3;


const uint16_t MIN_QUARTER_NOTE = 200;
const uint16_t MAX_QUARTER_NOTE = 2000; // max possible 4095 msec
