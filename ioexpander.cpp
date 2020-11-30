#include "ioexpander.h"
#include <MCP23017.h>
#include <BPUtil.h>
#include "config.h"
#include "innerSequencer.h"
#include <Debouncer.h>

const uint8_t ROT_ENC_A = (1 << 0);
const uint8_t ROT_ENC_B = (1 << 1);
const uint8_t ROT_ENC_AB = 0x3;
const uint8_t ROT_ENC_BUTTON = (1 << 2);
const uint8_t FTSW = (1 << 3);
const uint8_t BUTTON = (1 << 4);

const uint8_t INPUTS = ROT_ENC_A | ROT_ENC_B | ROT_ENC_BUTTON | FTSW | BUTTON;

const uint8_t INPUT_PORT = MCP23x17_PORTA;
const uint8_t LED_PORT = MCP23x17_PORTB;

const uint8_t RGB1 = 0x7;
const uint8_t RGB2 = 0x38;

const uint8_t R = 4;
const uint8_t G = 2;
const uint8_t B = 1;

const uint8_t RED = R;
const uint8_t YELLOW = R | G;
const uint8_t GREEN = G;
const uint8_t CYAN = G | B;
const uint8_t BLUE = B;
const uint8_t VIOLET = R | B;
const uint8_t WHITE = R | G | B;
const uint8_t BLACK = 0;


MCP23017 ioExpander(0x7);


struct TapDivideMode {
	uint8_t num;
	uint8_t denom;
	uint8_t rgb;
};

uint8_t curDivMode = 0;

TapDivideMode divideModes[] = {
	{1, 1, RED},	// quarter note
	{3, 4, YELLOW},	// dotted eigth
	{2, 3, GREEN },	// quarter note triplet
	{1, 2, CYAN},	// eigth
	{1, 3, BLUE},	// eigth note triplet
	{1, 4, VIOLET}	// sixteenth
};


void updateLEDs() {
	uint8_t rgb1 = divideModes[curDivMode].rgb;
	uint8_t rgb2 = R | B;

	ioExpander.digitalWrite_8(LED_PORT, ~rgb1 & ~(rgb2 << 3), RGB1 | RGB2);
}

class RotaryEncoder {
public:

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

		// divide by 4, but account for -3 to 3 all mapping to 0.
		int32_t prevRndVal = rawValue >= 0 ? rawValue / 4 : (rawValue - 3) / 4;
		rawValue += diff;
		int32_t rndVal = rawValue >= 0 ? rawValue / 4 : (rawValue - 3) / 4;

		_value = clamp<int16_t>(_value + rndVal - prevRndVal, minVal, maxVal);

		return wasChange;
	}

	int8_t value() {
		return _value;
	}

private:
	int32_t rawValue = 1;
	int8_t _value = 0;
	uint8_t stepsPerDetent;
	uint8_t prevAB;
	int8_t minVal;
	int8_t maxVal;
};

RotaryEncoder rotEnc;

Debouncer divButtonDebouncer;



void processIOExpander(uint32_t now) {
	bool intterrupt = !isBitHigh(IOEX_INT_READ, IOEX_INT);

	if(intterrupt) {
		uint8_t inputs = ioExpander.digitalRead_8(INPUT_PORT);

		uint8_t oldValue = rotEnc.value();
		bool wasMovement = rotEnc.update(inputs & ROT_ENC_AB);
		if(wasMovement && (oldValue != rotEnc.value() || rotEnc.value() == 1 || rotEnc.value() == 16)) {
			setNumSteps(now, rotEnc.value());
		}

		divButtonDebouncer.update(inputs & ROT_ENC_BUTTON, now);
		if(divButtonDebouncer.justPressed()) {
			curDivMode = (curDivMode + 1) % countof(divideModes);
			updateLEDs();
			setDivide(now, divideModes[curDivMode].num, divideModes[curDivMode].denom);
		}
	}
}


void initIOExpander() {
	ioExpander.begin();

	ioExpander.pullUp_8(INPUT_PORT, INPUTS);
	ioExpander.regWrite_8(MCP23x17_GPINTENA, INPUTS);
	ioExpander.pinMode_8(LED_PORT, ~(RGB1 | RGB2));

	uint8_t inputs = ioExpander.digitalRead_8(INPUT_PORT);
	rotEnc.init(4, inputs & ROT_ENC_AB, 16, 1, 16);
	updateLEDs();
}