#include <Arduino.h>
#include <Wire.h>
#include <MCP23017.h>


#define DEBUG

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


const int MIN_QUARTER_NOTE = 200;
const int MAX_QUARTER_NOTE = 2000;




static inline void setBitsHigh(volatile uint8_t & port, uint8_t mask){
	port |= mask;
}

static inline void clearBits(volatile uint8_t & port, uint8_t mask){
	port &= ~mask;
}

static inline void setMaskedBitsTo(volatile uint8_t & port, uint8_t mask, uint8_t value){
	port = (port & ~mask) | (value & mask);
}

static inline bool isBitHigh(volatile uint8_t & port, uint8_t bit){
	return bit & port;
}


#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

#ifdef DEBUG

	#define PRINT_VAR(var) do{ Serial.print(#var ": "); Serial.println(var);} while(0)

	char strBuff[128];

	void PRINTF(const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));

	void PRINTF(const char* fmt, ...){
		va_list vargs;
		va_start(vargs, fmt);

		vsnprintf(strBuff, sizeof(strBuff), fmt, vargs );
		Serial.print(strBuff);

		va_end(vargs);
	}

	#define PRINT(x) Serial.print(x)
	#define PRINTLN(x) Serial.println(x)


	#define ASSERT(cond, ...) do{												\
		if(!(cond)){															\
			_onFailedAssert( #cond, __FILE__, __LINE__, "" __VA_ARGS__);  		\
		}																		\
	} while(0)


	void _onFailedAssert(const char *cond, const char *file, int line, const char *usrFmt, ...)
		__attribute__ ((format (printf, 4, 5)));


	void _onFailedAssert(const char *cond, const char *file, int line, const char *usrFmt, ...)	{
		va_list vargs;
		va_start(vargs, usrFmt);

		PRINTF("ASSERTION FAILURE: %s evaluates to false.\n", cond);
		PRINTF("At %s:%d\n", file, line);

		if(usrFmt[0] != '\0'){ 
			vsnprintf(strBuff, sizeof(strBuff), usrFmt, vargs);
			Serial.println(strBuff);
		}

		va_end(vargs);
		while(1){}
	}


#else
	#define PRINT_VAR(var) ((void) sizeof(var))
	#define ASSERT(cond, ...) ((void) sizeof(cond))
	#define PRINT(x) ((void) sizeof(x))
	#define PRINTLN(x) ((void) sizeof(x))
#endif



///////////////////////////////////////// IO Expander daughter board ////

MCP23017 ioExpander(0x7);

///////////////////////////////////////// ADC ///////

volatile bool adcValAvail;
volatile int adcVal; // never read without checking adcValAvail


ISR(ADC_vect){
	// Must read low first
	adcVal = ADCL | (ADCH << 8);

	adcValAvail = true;
}


void initADC(){
  	// Set MUX3..0 in ADMUX to read from AD3
	setMaskedBitsTo(ADMUX, 0xF, RATE_ADC_CHANNEL);

	// Set the Prescaler to 128 (16000KHz/128 = 125KHz) (Slowest possible)
	// Above 200KHz 10-bit results are not reliable.
	setMaskedBitsTo(ADCSRA, 0x7, 0x7);

	// ADEN: enable ADC
	// ADIE: enable interrupt
	setBitsHigh(ADCSRA, bit(ADEN) | bit(ADIE));
}

void startAdcRead(){
	adcValAvail = false;
	setBitsHigh(ADCSRA, bit(ADSC));
}


//////////////// Rate Pot

// rateVal ranges from 0 -> 1023-rateHysteresis
int rateVal = -1000; // force first read to produce a change
const int rateHysteresis = 4;

void checkRateVal(uint32_t now){
	// Serial.println(__func__);
	if(!adcValAvail){
		return;
	}


	//// apply hysteresis ////

	if(adcVal < rateVal){
		rateVal = adcVal;
	}else if(adcVal - rateHysteresis > rateVal){
		rateVal = adcVal - rateHysteresis;
	}else{
		// no change
		goto finish;
	}


	//// map rateVal to qtrNote duration ////
	{
		int qtrNote;
		const int maxRateVal = 1023-rateHysteresis;
		const int halfWay = map(900, 0, 1000, MAX_QUARTER_NOTE, MIN_QUARTER_NOTE);

		if(rateVal <= maxRateVal/2){
			qtrNote = map(rateVal, 0, maxRateVal/2, MAX_QUARTER_NOTE, halfWay);
		}else{
			qtrNote = map(rateVal, maxRateVal/2 + 1, maxRateVal, halfWay, MIN_QUARTER_NOTE);
		}

		setQuarterNote(qtrNote, now);
	}
finish:
	startAdcRead();
}


////////////////////////////////


class Sequencer{
public:

	void process(uint32_t now){
		while(now - lastStep >= durations[stepNum]){
			lastStep += durations[stepNum];
			stepNum = (stepNum + 1) % numSteps;
		}
		updatePins();
	}

	void setNewPhase(uint32_t startTime, uint32_t now){
		lastStep = startTime;
		stepNum = 0;
		process(now);
	}

	void resetPhase(uint32_t now){
		lastStep = now;
		stepNum = 0;
		updatePins();
	}

	uint16_t getDurationSoFar(uint32_t now){
		uint16_t soFar = 0;

		for(int8_t i=0; i<stepNum; i++){
			soFar += durations[i];
		}
		soFar += (now - lastStep);

		return soFar;
	}

	uint16_t getDurationTotal(){
		uint16_t total = 0;

		for(int8_t i=0; i<numSteps; i++){
			total += durations[i];
		}

		return total;
	}

	void setDurations(uint16_t *durations, uint8_t len){
		memcpy(this->durations, durations, len*sizeof(durations[0]));
		numSteps = len;
	}

	virtual void updatePins() = 0;

	// only public for debugging
	uint32_t lastStep = 0;
	uint8_t stepNum = 0;
	uint8_t numSteps;
	uint16_t durations[16];
};


class MuxSequencer : public Sequencer{
public:
	virtual void updatePins(){
		setMaskedBitsTo(MUX_WRITE, MUX_ADDR, stepNum);	
	}
};


class ClockSequencer : public Sequencer{
public:
	virtual void updatePins(){
		if(stepNum == 0){
			setBitsHigh(CLK_OUT_WRITE, CLK_OUT);
		}else{
			clearBits(CLK_OUT_WRITE, CLK_OUT);
		}
	}
};

MuxSequencer muxSequencer;

ClockSequencer clockSequencer;


uint16_t _quarterNote = 0;

void setQuarterNoteAt(uint16_t qtrNote, uint32_t at, uint32_t now){
	uint16_t clk_durationSoFar = clockSequencer.getDurationSoFar(now);
	int16_t offset;
	if(clk_durationSoFar < _quarterNote/2){
		offset = -clk_durationSoFar;
	}else{
		offset = _quarterNote - clk_durationSoFar;
	}
	offset += (now - at);
	setQuarterNoteWithOffset(qtrNote, offset, now);
}


void setQuarterNoteWithOffset(uint16_t qtrNote, int16_t offset, uint32_t now){
	PRINTLN(__func__);
	PRINT_VAR(qtrNote);
	PRINT_VAR(offset);
	PRINT_VAR(now);

	_quarterNote = qtrNote;

	//// calc getDurationSoFar and getDurationTotal. these are for
	//// phase calculation later
	uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
	uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

	// mux_durationSoFar = mu

	PRINT_VAR(mux_durationSoFar);
	PRINT_VAR(mux_durationTotal);


	///// update mux durations
	uint16_t mux_durations[16];
	for(uint8_t i=0; i<countof(mux_durations); i++){
		mux_durations[i] = qtrNote;
	}
	muxSequencer.setDurations(mux_durations, countof(mux_durations));

	//// update clk durations
	uint16_t clk_durations[2];
	clk_durations[0] = min(200, qtrNote/2);
	clk_durations[1] = qtrNote - clk_durations[0];
	clockSequencer.setDurations(clk_durations, countof(clk_durations));

	////// set phase
	if(mux_durationTotal == 0){ // first time set
		clockSequencer.resetPhase(now);
		muxSequencer.resetPhase(now);
	}else{
		uint32_t adj_durationSoFar = (mux_durationSoFar + offset) % mux_durationTotal;
		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
		uint32_t newStartTime = now - mux_newDurationSoFar;

		muxSequencer.setNewPhase(newStartTime, now);
		clockSequencer.setNewPhase(newStartTime, now);
	}
}

void setQuarterNote(uint16_t qtrNote, uint32_t now){
	setQuarterNoteWithOffset(qtrNote, 0, now);
}


void processSteps(uint32_t now){
	muxSequencer.process(now);
	clockSequencer.process(now);

	if(clockSequencer.stepNum == 0){
		ASSERT(muxSequencer.lastStep == clockSequencer.lastStep, 
				"ms.ls: %lu, cs.ls %lu", muxSequencer.lastStep, clockSequencer.lastStep );
	}
}



class Debouncer{
public:
	void update(bool newInput, uint32_t now){
		_prevPressed = _pressed;
		_prevInput = _input;
		_input = newInput;

		if(now - _lastTransition >= THRESHOLD){
			_pressed = !_input;
		}
		if(_input != _prevInput){
			_lastTransition = now;
		}
	}


	inline bool justPressed(){
		return _pressed && !_prevPressed;
	}

	inline bool pressed(){
		return _pressed;
	}

	inline bool justReleased(){
		return !_pressed && _prevPressed;
	}


private:
	const static uint8_t THRESHOLD = 50;

	uint32_t _lastTransition = -THRESHOLD;
	bool _input = true;
	bool _prevInput = true;
	bool _pressed = false;
	bool _prevPressed = false;
};

Debouncer tapDebouncer;



template <typename T, uint8_t SIZE>
class LastNBuffer{
public:
	LastNBuffer(T init){
		for(uint8_t i=0; i<SIZE; i++){
			data[i] = init;
		}
	}

	inline void push(T e){
		head = (head+1) % SIZE;
		data[head] = e;
	}

	inline T peek(uint8_t offset){
		ASSERT(offset < SIZE);
		return data[(head - offset + SIZE) % SIZE];
	}

	inline uint8_t size(){
		return SIZE;
	}
private:
	uint8_t head = 0;
	T data[SIZE];
};


LastNBuffer<uint32_t, 4> tapHistory(-MAX_QUARTER_NOTE);

bool withinPercent(int32_t x, int32_t y, int8_t percent){
	int32_t percentOff = (abs(x-y)*100) / x;
	return percentOff <= percent;
}

void onTap(uint32_t now){
	tapHistory.push(now);


	uint16_t firstDelta = now-tapHistory.peek(1);
	if(firstDelta > MAX_QUARTER_NOTE){
		return;
	}
	if(firstDelta < MIN_QUARTER_NOTE){
		PRINTLN("super quick press");
	}

	uint8_t intvalCnt = 1;

	for(; intvalCnt<tapHistory.size()-1; intvalCnt++){
		uint16_t curDelta = tapHistory.peek(intvalCnt)-tapHistory.peek(intvalCnt+1);
		if(!withinPercent(firstDelta, curDelta, 10)){
			break;
		}
	}

	PRINT_VAR(intvalCnt);

	uint16_t qtrNote = (now - tapHistory.peek(intvalCnt)) / (intvalCnt);
	setQuarterNoteAt(qtrNote, now, now);
}


void processTap(uint32_t now){
	tapDebouncer.update(isBitHigh(nTAP_READ, nTAP), now);

	if(tapDebouncer.justPressed()){
		PRINTLN("pressed");
	}
	if(tapDebouncer.justReleased()){
		PRINTLN("released");
	}


	if(tapDebouncer.justPressed()){
		onTap(now);
	}
}


void loop(){
	uint32_t now = millis();
	checkRateVal(now);
	processSteps(now);
	processTap(now);
	processIOExpander();

	static uint32_t lastMillis = now;
	static uint32_t lastMillis_fast = now;
	static uint32_t iter = 0;

	static uint16_t maxFrame = 0;
	maxFrame = max(maxFrame, now - lastMillis_fast);
	lastMillis_fast= now;
	
	if(++iter %10000 == 0){
		PRINT(iter/10000);
		PRINTF(": %lu ms, max frame: %u ms\n", now-lastMillis, maxFrame);
		maxFrame = 0;
		lastMillis = now;
	}

}


const uint8_t ROT_ENC_A = (1 << 0);
const uint8_t ROT_ENC_B = (1 << 1);
const uint8_t ROT_ENC_BUTTON = (1 << 2);
const uint8_t FTSW = (1 << 3);
const uint8_t BUTTON = (1 << 4);

const uint8_t INPUTS = ROT_ENC_A | ROT_ENC_B | ROT_ENC_BUTTON | FTSW | BUTTON;

const uint8_t RGB1 = 0x7;
const uint8_t RGB2 = 0x38;

const uint8_t INPUT_PORT = IOEX16_PORTA;
const uint8_t LED_PORT = IOEX16_PORTB;

// ISR(INT0_vect){
// 		PRINT("ISR: ");
// 		PRINT_VAR(isBitHigh(PORTD, 1 << 2));
// }


void processIOExpander(){
	static bool prevInt0 = 3;

	bool int0 = isBitHigh(PIND, 1 << 2);



	if(int0 == 0){
		volatile uint8_t iocap = ioExpander.read_8(MCP23x17_INTCAPA);
		volatile uint8_t inputs = ioExpander.read_8(MCP23x17_GPIOA);
		// PRINTF("cap: %x, inputs: %x\n", (unsigned int) iocap, (unsigned int) inputs);
	}
}


void initIOExpander(){
	ioExpander.begin();

	ioExpander.pullUp_8(INPUT_PORT, INPUTS);
	ioExpander.write_8(MCP23x17_GPINTENA, INPUTS);
	ioExpander.pinMode_8(LED_PORT, ~(RGB1 | RGB2));
}


void setup(){
	#ifdef DEBUG
	Serial.begin(115200);
	#endif
	PRINTLN("--------------- Step Sequencer Start --------------");

	setBitsHigh(MUX_DDR, POT_nENABLE | LED_nENABLE | MUX_ADDR) ;
	setBitsHigh(CLK_OUT_DDR, CLK_OUT);

	setBitsHigh(nTAP_WRITE, nTAP);

	initADC();
	startAdcRead();

	initIOExpander();



	while(!adcValAvail){} // force ADC read before anything else
}

