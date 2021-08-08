#include "ioexpander.h"
#include <MCP23017.h>
#include <BPUtil.h>
#include "config.h"
#include "sequencePlayers.h"
#include <Debouncer.h>

const uint8_t ROT_ENC_A = (1 << 0);
const uint8_t ROT_ENC_B = (1 << 1);
const uint8_t ROT_ENC_AB = 0x3;
const uint8_t ROT_ENC_BUTTON = (1 << 2);
const uint8_t PHASE_RESET_BUTTON = (1 << 3);
const uint8_t RHYTHM_BUTTON = (1 << 4);

const uint8_t INPUTS = ROT_ENC_A | ROT_ENC_B | ROT_ENC_BUTTON | PHASE_RESET_BUTTON | RHYTHM_BUTTON;

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

uint8_t curDivMode = 1;

TapDivideMode divideModes[] = {
	{2, 1, WHITE},	// half-note
	{1, 1, RED},	// quarter note
	{3, 4, YELLOW},	// dotted eigth
	{2, 3, GREEN },	// quarter note triplet
	{1, 2, CYAN},	// eigth
	{1, 3, BLUE},	// eigth note triplet
	{1, 4, VIOLET},	// sixteenth
	{0, 0, BLACK}	// single step mode
};




struct RhythmMode {
	uint16_t rhythm[MAX_STEPS];
	uint8_t rgb;
};

uint8_t curRhythmMode = 0;

RhythmMode rhythmModes[] = {
	{{	1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000,
		1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000}, RED},

	{{	1300,  700, 1300, 700, 		1300,  700, 1300, 700,
		1300,  700, 1300, 700, 		1300,  700, 1300, 700}, YELLOW},

	{{	1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000,
		1333, 1333, 1334, 			1000, 1000, 1000,  500,  500}, GREEN},

	{{	 500, 1000, 1000,  500, 1000,
		 500, 1000, 500, 500,  333,  333,  334, 500,
		 2000, 1000, 1000}, CYAN},

	{{	1000, 1750,  250, 1000, 	 500,  500, 1750, 1250,
		 500, 1500,  2000, 			 1333, 445, 444, 445, 1333}, BLUE},

	{{	1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000,
		1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000}, VIOLET},

	{{	1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000,
		1000, 1000, 1000, 1000, 	1000, 1000, 1000, 1000}, WHITE}
};


void updateLEDs() {
	uint8_t rgb1 = divideModes[curDivMode].rgb;
	uint8_t rgb2 = rhythmModes[curRhythmMode].rgb;

	ioExpander.digitalWrite_8(LED_PORT, ~rgb1 & ~(rgb2 << 3), RGB1 | RGB2);
}



static inline int32_t floorDiv(int32_t x, uint8_t y){
	return (x > 0) ? (x/y) : (x-y+1)/y;
}


class RotaryEncoder {
public:

	void init(uint8_t stepsPerDetent, uint8_t startAB, int8_t startVal, int8_t minVal, int8_t maxVal) {
		_prevAB = startAB;
		_stepsPerDetent = stepsPerDetent;
		_minVal = minVal;
		_maxVal = maxVal;
		_value = startVal;
	}

	bool update(uint8_t ab) {
		const int8_t RAW_VALUES_TO_DIFF[] = {
			//			00,	01, 10, 11
			/* 00 */	 0,	+1,	-1,	 0,
			/* 01 */	-1,	 0,	 0,	+1,
			/* 10 */	+1,	 0,  0, -1,
			/* 11 */	 0, -1, +1,  0

		};

		int8_t diff = RAW_VALUES_TO_DIFF[(_prevAB << 2) | ab];
		int32_t prevRndVal = floorDiv(_rawValue, 4);
		_rawValue += diff;
		int32_t rndVal = floorDiv(_rawValue, 4);

		_value = clamp<int16_t>(_value + rndVal - prevRndVal, _minVal, _maxVal);

		bool wasChange = (_prevAB != ab); 	// detect any encoder "movement", as oppposed to
											// change in output value, which is clamped.
		_prevAB = ab;
		return wasChange;
	}

	int8_t value() {
		return _value;
	}

private:
	int32_t _rawValue = 1;
	int8_t _value;
	uint8_t _stepsPerDetent;
	uint8_t _prevAB;
	int8_t _minVal;
	int8_t _maxVal;
};

RotaryEncoder rotEnc;

Debouncer divButtonDebouncer;

Debouncer resetButtonDebouncer;

Debouncer rhythmButtonDebouncer;


void processIOExpander(uint32_t now) {
	uint8_t inputs = ioExpander.digitalRead_8(INPUT_PORT);

	uint8_t oldValue = rotEnc.value();
	bool wasMovement = rotEnc.update(inputs & ROT_ENC_AB);
	if(wasMovement && (oldValue != rotEnc.value() || rotEnc.value() == 1 || rotEnc.value() == MAX_STEPS)) {
		setNumSteps(now, rotEnc.value());
	}


	divButtonDebouncer.update(inputs & ROT_ENC_BUTTON, now);
	if(divButtonDebouncer.justPressed()) {
		curDivMode = (curDivMode + 1) % countof(divideModes);
		updateLEDs();
		if(divideModes[curDivMode].num == 0) {
			setSingleStepMode(true);
		}
		else {
			setSingleStepMode(false);
			setDivide(now, divideModes[curDivMode].num, divideModes[curDivMode].denom);
		}
	}

	resetButtonDebouncer.update(inputs & PHASE_RESET_BUTTON, now);
	if(resetButtonDebouncer.justPressed()) {
		PRINTLN("reset button pressed");
		if(inSingleStepMode()) {
			singleStep();
		}
		else {
			resetPhase(now);
		}
	}

	rhythmButtonDebouncer.update(inputs & RHYTHM_BUTTON, now);
	if(rhythmButtonDebouncer.justPressed()) {
		PRINTLN("rhythm button pressed");
		curRhythmMode = (curRhythmMode + 1) % countof(rhythmModes);
		updateLEDs();
		setRhythm(now, &(rhythmModes[curRhythmMode].rhythm));
	}
}


void initIOExpander() {
	ioExpander.begin();

	ioExpander.pullUp_8(INPUT_PORT, INPUTS);
	ioExpander.regWrite_8(MCP23x17_GPINTENA, INPUTS);
	ioExpander.pinMode_8(LED_PORT, ~(RGB1 | RGB2));

	uint8_t inputs = ioExpander.digitalRead_8(INPUT_PORT);
	rotEnc.init(4, inputs & ROT_ENC_AB, MAX_STEPS, 1, MAX_STEPS);
	updateLEDs();
}