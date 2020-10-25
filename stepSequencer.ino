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

void setup(){
	Serial.begin(115200);
	Serial.println("--------------- Step Sequencer Start --------------");



	setBitsHigh(DDRB, POT_nENABLE | LED_nENABLE | MUX_ADDR) ;
	setBitsHigh(DDRD, CLK_OUT);

	initADC();
	startAdcRead();
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


int rateVal = 0; // ranges from 0 -> 1023-rateHysteresis
const int rateHysteresis = 4;

void checkRateVal(){
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

int _quarterNote = 500;
uint32_t _mux_lastStep = 0;

uint8_t _mux_stepNum = 0;
uint8_t _mux_numSteps = 16;


uint32_t _clk_lastStep = 0;
uint8_t _clk_stepNum = 0;
uint8_t _clk_numSteps = 2;

uint16_t _clk_durations[2] = {1000, 1000};


void setQuarterNote(int quarterNote){
	PRINT(quarterNote);

	_quarterNote = quarterNote;

	// _clk_durations[0] = min(200, quarterNote/2);
	_clk_durations[0] = quarterNote/2;
	_clk_durations[1] = quarterNote - _clk_durations[0];

	_resetPhase();
}


void _resetPhase(){
	Serial.println(__func__);



	_clk_stepNum = _clk_numSteps - 1;
	_mux_stepNum = _mux_numSteps - 1;

	_mux_step();
	_clk_step();

	uint32_t now = millis();

	_clk_lastStep = now;
	_mux_lastStep = now;

	PRINT(_clk_lastStep);
	PRINT(_mux_lastStep);
	PRINT(_quarterNote);
	PRINT(_clk_durations[0]);
	PRINT(_clk_durations[1]);

}


void _mux_step(){
	_mux_stepNum = (_mux_stepNum+1) % _mux_numSteps;
	setMaskedBitsTo(MUX_PORT, MUX_ADDR, _mux_stepNum);
}

void _clk_step(){
	_clk_stepNum = (_clk_stepNum+1) % _clk_numSteps;
	if(_clk_stepNum == 0){
		setBitsHigh(CLK_OUT_PORT, CLK_OUT);
	}else{
		clearBits(CLK_OUT_PORT, CLK_OUT);
	}
}

void processSteps(){
	uint32_t now = millis();

	if(now - _mux_lastStep >= _quarterNote){
		_mux_step();
		_mux_lastStep += _quarterNote;
	}


	if(now - _clk_lastStep >= _clk_durations[_clk_stepNum]){
		_clk_lastStep += _clk_durations[_clk_stepNum];
		_clk_step();
		if(_clk_stepNum == 0){
			ASSERT(_mux_lastStep == _clk_lastStep, "(%lu) %lu, %lu", now, _mux_lastStep, _clk_lastStep);
		}
	}
}


// Processor loop
void loop(){
	checkRateVal();
	processSteps();

}


