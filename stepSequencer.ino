#include "config.h"
#include "ratePot.h"
#include "tapHandler.h"
#include "innerSequencer.h"
#include <Arduino.h>
#include <BPUtil.h>
#include "ioexpander.h"



void measurePerformance(uint32_t now){
	static uint32_t lastMillis = now;
	static uint32_t lastMillis_fast = now;
	static uint32_t iter = 0;

	static uint16_t maxFrame = 0;
	maxFrame = max(maxFrame, now - lastMillis_fast);
	lastMillis_fast= now;

	if(++iter %100000 == 0){
		PRINT(iter/100000);
		PRINTF(": %lu ms, max frame: %u ms\n", now-lastMillis, maxFrame);
		maxFrame = 0;
		lastMillis = now;
	}
}



void loop(){
	// PRINTLN("loop");
	uint32_t now = millis();
	processRatePot(now);
	processInnerSequencer(now);
	processTap(now);
	processIOExpander(now);

	measurePerformance(now);

}





void setup(){
	#ifdef BPDEBUG
	Serial.begin(115200);
	#endif
	PRINTLN("--------------- Step Sequencer Start --------------");

	initInnerSequencer();
	initTapHandler();
	initRatePot();
	initIOExpander();
}

