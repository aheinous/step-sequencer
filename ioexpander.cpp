#include "ioexpander.h"
#include <MCP23017.h>
#include <BPUtil.h>
#include "config.h"
#include "innerSequencer.h"

const uint8_t ROT_ENC_A = (1 << 0);
const uint8_t ROT_ENC_B = (1 << 1);
const uint8_t ROT_ENC_AB = 0x3;
const uint8_t ROT_ENC_BUTTON = (1 << 2);
const uint8_t FTSW = (1 << 3);
const uint8_t BUTTON = (1 << 4);

const uint8_t INPUTS = ROT_ENC_A | ROT_ENC_B | ROT_ENC_BUTTON | FTSW | BUTTON;

const uint8_t RGB1 = 0x7;
const uint8_t RGB2 = 0x38;

const uint8_t INPUT_PORT = MCP23x17_PORTA;
const uint8_t LED_PORT = MCP23x17_PORTB;


MCP23017 ioExpander(0x7);


class RotaryEncoder{
public:

	// RotaryEncoder()
	// {}

	void init(uint8_t stepsPerDetent, uint8_t startAB, int8_t startVal, int8_t minVal, int8_t maxVal) {
		prevAB = startAB;
		this->stepsPerDetent = stepsPerDetent;
		this->minVal = minVal;
		this->maxVal = maxVal;
		_value = startVal;
	}

	bool update(uint8_t ab) {
		uint8_t idx = (prevAB << 2) | ab;

		const int8_t LUT[] = {
			//			00,	01, 10, 11
			/* 00 */	 0,	+1,	-1,	 0,
			/* 01 */	-1,	 0,	 0,	+1,
			/* 10 */	+1,	 0,  0, -1,
			/* 11 */	 0, -1, +1,  0

		};

		int8_t diff = LUT[idx];
		bool wasChange = (prevAB != ab); // detect any encoder "movement" not change in output value
		prevAB = ab;

		int32_t prevRndVal = rawValue >= 0 ? rawValue / 4 : (rawValue - 3) / 4;
		rawValue += diff;
		int32_t rndVal = rawValue >= 0 ? rawValue / 4 : (rawValue - 3) / 4;

		// int16_t prevValue = _value;

		_value = clamp<int16_t>(_value + rndVal - prevRndVal, minVal, maxVal);


		// // PRINTF("ab: %x\n")
		// PVAR(ab);
		// // if(hystValue.update(rawValue)){
		// PVAR(rawValue);
		// PVAR(prevRndVal);
		// PVAR(rndVal);

		// // }

		// PVAR(value());
		// PRINTLN();

		// return prevValue != _value;

		return wasChange;

	}

	int8_t value() {
		return _value;
	}

private:
	// Hysteresis<int32_t> hystValue;
	int32_t rawValue = 1;
	int16_t _value = 0;
	uint8_t stepsPerDetent;
	uint8_t prevAB;
	int16_t minVal;
	int16_t maxVal;
	// uint8_t
};

RotaryEncoder rotEnc;

void processIOExpander(){
	// static bool prevInt0 = 3;

	bool int0 = isBitHigh(IOEX_INT_READ, IOEX_INT);

	// PVAR(int0);


	if(int0 == 0) {
		// volatile uint8_t iocap = ioExpander.regRead_8(MCP23x17_INTCAPA);
		uint8_t inputs = ioExpander.digitalRead_8(INPUT_PORT);

		uint8_t oldValue = rotEnc.value();
		bool wasMovement = rotEnc.update(inputs & ROT_ENC_AB);
		if(wasMovement && (oldValue != rotEnc.value() || rotEnc.value() == 1 || rotEnc.value() == 16)) {
			setNumSteps(now, rotEnc.value());
		}

	}
}


void initIOExpander(){
	ioExpander.begin();

	ioExpander.pullUp_8(INPUT_PORT, INPUTS);
	ioExpander.regWrite_8(MCP23x17_GPINTENA, INPUTS);
	ioExpander.pinMode_8(LED_PORT, ~(RGB1 | RGB2));

	uint8_t inputs = ioExpander.digitalRead_8(INPUT_PORT);
	rotEnc.init(4, inputs & ROT_ENC_AB, 16, 1, 16);

}