#include "config.h"
#include "ratePot.h"
#include "tapHandler.h"
#include "innerSequencer.h"
#include <Arduino.h>
#include <BPUtil.h>
#include "ioexpander.h"


void measurePerformance(uint32_t now){
	static uint32_t lastMillis = now;
	static uint32_t lastFrameMicros = micros();
	static uint32_t iter = 0;


	uint32_t now_us = micros();
	static uint16_t maxFrame = 0;
	maxFrame = max(maxFrame, now_us - lastFrameMicros);
	lastFrameMicros= now_us;

	if(++iter %10000 == 0){
		PRINT(iter);
		PRINTF(": %lu ms, max frame: %u usec\n", now-lastMillis, maxFrame);
		maxFrame = 0;
		lastMillis = now;
	}
}


void loop(){
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

