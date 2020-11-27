#include "ioexpander.h"
#include <MCP23017.h>

const uint8_t ROT_ENC_A = (1 << 0);
const uint8_t ROT_ENC_B = (1 << 1);
const uint8_t ROT_ENC_BUTTON = (1 << 2);
const uint8_t FTSW = (1 << 3);
const uint8_t BUTTON = (1 << 4);

const uint8_t INPUTS = ROT_ENC_A | ROT_ENC_B | ROT_ENC_BUTTON | FTSW | BUTTON;

const uint8_t RGB1 = 0x7;
const uint8_t RGB2 = 0x38;

const uint8_t INPUT_PORT = MCP23x17_PORTA;
const uint8_t LED_PORT = MCP23x17_PORTB;


MCP23017 ioExpander(0x7);

void processIOExpander(){
	// static bool prevInt0 = 3;

	// bool int0 = isBitHigh(PIND, 1 << 2);



	// if(int0 == 0){
	// 	volatile uint8_t iocap = ioExpander.read_8(MCP23x17_INTCAPA);
	// 	volatile uint8_t inputs = ioExpander.read_8(MCP23x17_GPIOA);
	// 	// PRINTF("cap: %x, inputs: %x\n", (unsigned int) iocap, (unsigned int) inputs);
	// }
}


void initIOExpander(){
	ioExpander.begin();

	ioExpander.pullUp_8(INPUT_PORT, INPUTS);
	ioExpander.regWrite_8(MCP23x17_GPINTENA, INPUTS);
	ioExpander.pinMode_8(LED_PORT, ~(RGB1 | RGB2));
}