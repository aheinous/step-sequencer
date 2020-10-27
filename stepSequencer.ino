#include <Arduino.h>


const uint8_t CLK_OUT = 1<<4;
volatile uint8_t & CLK_OUT_PORT = PORTD;

const uint8_t POT_nENABLE = 1<<5;
const uint8_t LED_nENABLE = 1<<4;
const uint8_t MUX_ADDR = 0xF;
volatile uint8_t & MUX_PORT = PORTB;


int ratePin = A3;


const int MIN_QUARTER_NOTE = 200;
const int MAX_QUARTER_NOTE = 2000;



#define PRINT(var) do{ Serial.print(#var ": "); Serial.println(var);} while(0)

static inline void setBitsHigh(volatile uint8_t & port, uint8_t mask){
	port |= mask;
}

static inline void clearBits(volatile uint8_t & port, uint8_t mask){
	port &= ~mask;
}

static inline void setMaskedBitsTo(volatile uint8_t & port, uint8_t mask, uint8_t value){
		port = (port & ~mask) | (value & mask);
}


#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

#define ASSERT(cond, ...) do{												\
	if(!(cond)){															\
		_onFailedAssert( #cond, __FILE__, __LINE__, "" __VA_ARGS__);  		\
	}																		\
} while(0)



void _onFailedAssert(const char *cond, const char *file, int line, ...){
	va_list vargs;
	va_start(vargs, line);

	static char buff[256];

	snprintf(buff, sizeof(buff), "ASSERTION FAILURE: %s evaluates to false.\n", cond);
	Serial.print(buff);	
	snprintf(buff, sizeof(buff), "At %s:%d\n", file, line);
	Serial.print(buff);

	const char *usrFmt = va_arg(vargs, const char*);
	if(usrFmt[0] != '\0'){ 
		vsnprintf(buff, sizeof(buff), usrFmt, vargs);
		Serial.println(buff);
	}

	va_end(vargs);
	while(1){}
}




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
	setMaskedBitsTo(ADMUX, 0xF, 3);

	// Set the Prescaler to 128 (16000KHz/128 = 125KHz) (Slowest possible)
	// Above 200KHz 10-bit results are not reliable.
	setMaskedBitsTo(ADCSRA, 0x7, 0x7);

	// ADEN: enable ADC
	// ADIE: enable interrupt
	setBitsHigh(ADCSRA, bit(ADEN) | bit(ADIE));

	interrupts();
}

void startAdcRead(){
	adcValAvail = false;
	setBitsHigh(ADCSRA, bit(ADSC));
}


//////////////// Rate Pot

// rateVal ranges from 0 -> 1023-rateHysteresis
int rateVal = -1000; // force first read to produce a change
const int rateHysteresis = 4;

void checkRateVal(){
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
		startAdcRead();
		return;
	}


	//// map rateVal to qtrNote duration ////

	int qtrNote;
	const int maxRateVal = 1023-rateHysteresis;
	const int halfWay = map(900, 0, 1000, MAX_QUARTER_NOTE, MIN_QUARTER_NOTE);

	if(rateVal <= maxRateVal/2){
		qtrNote = map(rateVal, 0, maxRateVal/2, MAX_QUARTER_NOTE, halfWay);
	}else{
		qtrNote = map(rateVal, maxRateVal/2 + 1, maxRateVal, halfWay, MIN_QUARTER_NOTE);
	}

	setQuarterNote(qtrNote);
	startAdcRead();
}


////////////////////////////////


class Sequencer{
public:

	void process(uint32_t now){
		ASSERT(numSteps);
		ASSERT(numSteps <= countof(durations));

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



	uint32_t lastStep = 0;
	uint8_t stepNum = 0;
	uint8_t numSteps;
	uint16_t durations[16];
};


class MuxSequencer : public Sequencer{
public:
	virtual void updatePins(){
		setMaskedBitsTo(MUX_PORT, MUX_ADDR, stepNum);	
	}
};


class ClockSequencer : public Sequencer{
public:
	virtual void updatePins(){
		if(stepNum == 0){
			setBitsHigh(CLK_OUT_PORT, CLK_OUT);
		}else{
			clearBits(CLK_OUT_PORT, CLK_OUT);
		}
	}
};

MuxSequencer muxSequencer;

ClockSequencer clockSequencer;




void setQuarterNote(int qtrNote){
	Serial.println(__func__);
	PRINT(qtrNote);

	uint32_t now = millis();

	//// calc getDurationSoFar and getDurationTotal. these are for
	//// phase calculation later
	uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
	uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

	PRINT(mux_durationSoFar);
	PRINT(mux_durationTotal);


	///// update mux durations
	uint16_t mux_durations[16];
	for(int8_t i=0; i<countof(mux_durations); i++){
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
		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(mux_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
		uint32_t newStartTime = now - mux_newDurationSoFar;

		muxSequencer.setNewPhase(newStartTime, now);
		clockSequencer.setNewPhase(newStartTime, now);
	}
}


void processSteps(){
	uint32_t now = millis();
	muxSequencer.process(now);
	clockSequencer.process(now);

	if(clockSequencer.stepNum == 0){
		ASSERT(muxSequencer.lastStep == clockSequencer.lastStep);
	}
}


void loop(){
	checkRateVal();
	processSteps();

}

void setup(){
	Serial.begin(115200);
	Serial.println("--------------- Step Sequencer Start --------------");



	setBitsHigh(DDRB, POT_nENABLE | LED_nENABLE | MUX_ADDR) ;
	setBitsHigh(DDRD, CLK_OUT);

	initADC();
	startAdcRead();

	while(!adcValAvail){} // force ADC read before anything else
}



