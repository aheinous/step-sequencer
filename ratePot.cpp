#include "ratePot.h"
#include "config.h"
#include "innerSequencer.h"
#include <BPUtil.h>

///////////////////////////////////////// ADC ///////

volatile bool adcValAvail;
volatile int adcVal; // never read without checking adcValAvail


ISR(ADC_vect){
	// Must read low first
	adcVal = ADCL | (ADCH << 8);

	adcValAvail = true;
}


static void initADC(){
  	// Set MUX3..0 in ADMUX to read from AD3
	setMaskedBitsTo(ADMUX, 0xF, RATE_ADC_CHANNEL);

	// Set the Prescaler to 128 (16000KHz/128 = 125KHz) (Slowest possible)
	// Above 200KHz 10-bit results are not reliable.
	setMaskedBitsTo(ADCSRA, 0x7, 0x7);

	// ADEN: enable ADC
	// ADIE: enable interrupt
	setBitsHigh(ADCSRA, bit(ADEN) | bit(ADIE));
}

static void startAdcRead(){
	adcValAvail = false;
	setBitsHigh(ADCSRA, bit(ADSC));
}


//////////////// Rate Pot

// rateVal ranges from 0 -> 1023-rateHysteresis
int rateVal = -1000; // force first read to produce a change
const int rateHysteresis = 4;

void processRatePot(uint32_t now){
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


void initRatePot(){
	initADC();
	startAdcRead();
	while(!adcValAvail){} // force ADC read read avail before first processRatePot() call
}