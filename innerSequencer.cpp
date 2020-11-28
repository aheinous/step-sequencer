#include "innerSequencer.h"
#include "config.h"
#include <BPUtil.h>

class Sequencer{
public:

	void process(uint32_t now){
		if(_blitzMode){
			_stepNum = (_stepNum + 1) % _numSteps;
		}
		else{
			while(now - _lastStep >= _durations[_stepNum]){
				_lastStep += _durations[_stepNum];
				_stepNum = (_stepNum + 1) % _numSteps;
			}
		}
		updatePins();
	}

	void setNumSteps(uint8_t n){
		_numSteps = n;
	}

	uint8_t numSteps(){
		return _numSteps;
	}

	void setBlitzMode(bool enabled){
		_blitzMode = enabled;
	}

	void setNewPhase(uint32_t startTime, uint32_t now){
		ASSERTF(now - startTime < (100000), "now: %lu, startTime: %lu", now, startTime);
		_lastStep = startTime;
		_stepNum = 0;
		process(now);
	}

	void resetPhase(uint32_t now){
		_lastStep = now;
		_stepNum = 0;
		updatePins();
	}

	uint16_t getDurationSoFar(uint32_t now){
		uint16_t soFar = 0;

		for(int8_t i=0; i<_stepNum; i++){
			soFar += _durations[i];
		}
		soFar += (now - _lastStep);

		return soFar;
	}

	uint16_t getDurationTotal(){
		uint16_t total = 0;

		for(int8_t i=0; i<_numSteps; i++){
			total += _durations[i];
		}

		return total;
	}


	uint16_t *durations(){
		return &_durations[0];
	}

	void printDurations(){
		for(uint8_t i =0; i<16; i++){
			PRINT(_durations[i]);
			PRINT(", ");
		}
		PRINTLN();
	}

	virtual void updatePins() = 0;

	// only public for debugging
	uint32_t _lastStep = 0;
	uint8_t _stepNum = 0;
	uint8_t _numSteps = 16;
	uint16_t _durations[16];
	bool _blitzMode = false;
};


class MuxSequencer : public Sequencer{
public:
	MuxSequencer(){
		_numSteps = 16;
	}

	virtual void updatePins(){
		setMaskedBitsTo(MUX_WRITE, MUX_ADDR, _stepNum);
	}
};


class ClockSequencer : public Sequencer{
public:
	ClockSequencer(){
		_numSteps = 2;
	}

	virtual void updatePins(){
		if(_stepNum == 0){
			setBitsHigh(CLK_OUT_WRITE, CLK_OUT);
		}else{
			clearBits(CLK_OUT_WRITE, CLK_OUT);
		}
	}
};

MuxSequencer muxSequencer;

ClockSequencer clockSequencer;


uint16_t _quarterNote = 0;

// uint8_t numSteps = 16;

int16_t showNumStepsFor = 0;




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


	PVAR(mux_durationSoFar);
	PVAR(mux_durationTotal);
	PVAR(muxSequencer.numSteps());

	///// update mux durations
	for(uint8_t i=0; i<muxSequencer.numSteps(); i++){
		muxSequencer.durations()[i] = qtrNote;
	}

	//// update clk durations
	clockSequencer.durations()[0] = min(200, qtrNote/2);
	clockSequencer.durations()[1] = qtrNote - clockSequencer.durations()[0];


	////// set phase
	if(mux_durationTotal == 0){ // first time set
		clockSequencer.resetPhase(now);
		muxSequencer.resetPhase(now);
	}else{
		uint32_t adj_durationSoFar = (mux_durationSoFar + offset) % mux_durationTotal;
		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
		uint32_t newStartTime = now - mux_newDurationSoFar;

		// PVAR(adj_durationSoFar);
		// PVAR(mux_newDurationTotal);
		// PVAR(mux_newDurationSoFar);
		// PVAR(now);
		// PVAR(newStartTime);

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

void setNumSteps(uint8_t n){
	PRINTF("set num steps: %u\n", n);
	setBitsHigh(MUX_WRITE, POT_nENABLE);
	showNumStepsFor = 1000;

	muxSequencer.setNumSteps(n);
	for(uint8_t i=0; i<n; i++){
		muxSequencer.durations()[i] = _quarterNote;
	}

	muxSequencer.setBlitzMode(true);
}


void processInnerSequencer(uint32_t now){
	static uint32_t prevNow = 0;
	if(showNumStepsFor){
		muxSequencer.process(now);
		showNumStepsFor -= (now - prevNow);
		if(showNumStepsFor <= 0){
			showNumStepsFor = 0;
			muxSequencer.setBlitzMode(false);
			muxSequencer.resetPhase(now);
			clockSequencer.resetPhase(now);
			clearBits(MUX_WRITE, POT_nENABLE);
			PRINT("Exit show num steps mode.\n");
			// PRINTF("now: %lu, ms.ls: %lu, cs.ls %lu, ms.stepnum: %u cs.stepnum: %u, ms.numSteps: %u\n",
			// 	now,
			// 	muxSequencer._lastStep, clockSequencer._lastStep,
			// 	muxSequencer._stepNum, clockSequencer._stepNum,
			// 	muxSequencer.numSteps());
			muxSequencer.printDurations();
			clockSequencer.printDurations();
		}
	}else{
		muxSequencer.process(now);
		clockSequencer.process(now);

		if(clockSequencer._stepNum == 0){
			ASSERTF(muxSequencer._lastStep == clockSequencer._lastStep,
					"now: %lu, ms.ls: %lu, cs.ls %lu, ms.stepnum: %u cs.stepnum: %u, ms.numSteps: %u",
					now,
					muxSequencer._lastStep, clockSequencer._lastStep,
					muxSequencer._stepNum, clockSequencer._stepNum,
					muxSequencer.numSteps() );
		}

	}
	prevNow = now;
}

void initInnerSequencer(){
	setBitsHigh(MUX_DDR, POT_nENABLE | LED_nENABLE | MUX_ADDR) ;
	setBitsHigh(CLK_OUT_DDR, CLK_OUT);
}