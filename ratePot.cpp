#include "ratePot.h"
#include "config.h"
#include "sequencePlayers.h"
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

//  hystPos.value() ranges from 0 -> 1023-hystPos.hysteresis
//  -1000 forces first read to produce a change

Hysteresis<int16_t> hystPot(4, -1000);

void processRatePot(uint32_t now){
	if(!adcValAvail){
		return;
	}

	if(hystPot.update(adcVal)){
		//// map rateVal to qtrNote duration ////
		int qtrNote;
		const int maxRateVal = 1023-hystPot.hysteresis;
		const int halfWay = map(900, 0, 1000, MAX_QUARTER_NOTE, MIN_QUARTER_NOTE);

		if(hystPot.value() <= maxRateVal/2){
			qtrNote = map(hystPot.value(), 0, maxRateVal/2, MAX_QUARTER_NOTE, halfWay);
		}else{
			qtrNote = map(hystPot.value(), maxRateVal/2 + 1, maxRateVal, halfWay, MIN_QUARTER_NOTE);
		}

		setQuarterNote_keepPhase(now, qtrNote);
	}
	startAdcRead();
}


void initRatePot(){
	initADC();
	startAdcRead();
	while(!adcValAvail){} // force ADC read read avail before first processRatePot() call
}