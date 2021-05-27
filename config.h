#pragma once

#include <stdint.h>
#define MAX_STEPS 16

extern const uint8_t CLK_OUT;
extern volatile uint8_t & CLK_OUT_WRITE;
extern volatile uint8_t & CLK_OUT_DDR;

extern const uint8_t POT_nENABLE;
extern const uint8_t LED_nENABLE;
extern const uint8_t MUX_ADDR;
extern volatile uint8_t & MUX_WRITE;
extern volatile uint8_t & MUX_DDR;


extern const uint8_t nTAP;
extern volatile uint8_t & nTAP_WRITE;
extern volatile uint8_t & nTAP_READ;
extern volatile uint8_t & nTAP_DDR;

extern const uint8_t IOEX_INT;
extern volatile uint8_t & IOEX_INT_READ;

extern uint8_t RATE_ADC_CHANNEL;


extern const uint16_t MIN_QUARTER_NOTE;
extern const uint16_t MAX_QUARTER_NOTE;
