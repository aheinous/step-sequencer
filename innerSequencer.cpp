#include "innerSequencer.h"
#include "config.h"
#include <BPUtil.h>

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




void setQuarterNoteWithOffset(uint16_t qtrNote, int16_t offset, uint32_t now){
	PRINTLN(__func__);
	PVAR(qtrNote);
	PVAR(offset);
	PVAR(now);

	_quarterNote = qtrNote;

	//// calc getDurationSoFar and getDurationTotal. these are for
	//// phase calculation later
	uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
	uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

	// mux_durationSoFar = mu

	PVAR(mux_durationSoFar);
	PVAR(mux_durationTotal);


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

void setQuarterNote(uint16_t qtrNote, uint32_t now){
	setQuarterNoteWithOffset(qtrNote, 0, now);
}


void processInnerSequencer(uint32_t now){
	muxSequencer.process(now);
	clockSequencer.process(now);

	if(clockSequencer.stepNum == 0){
		ASSERTF(muxSequencer.lastStep == clockSequencer.lastStep, 
				"ms.ls: %lu, cs.ls %lu", muxSequencer.lastStep, clockSequencer.lastStep );
	}
}

void initInnerSequencer(){
	setBitsHigh(MUX_DDR, POT_nENABLE | LED_nENABLE | MUX_ADDR) ;
	setBitsHigh(CLK_OUT_DDR, CLK_OUT);
}