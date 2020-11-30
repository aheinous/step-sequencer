#include "innerSequencer.h"
#include "config.h"
#include <BPUtil.h>

class Sequencer {
public:

	void process(uint32_t now) {
		ASSERT(_stepNum < _numSteps);
		ASSERT(_numSteps <= 16 && _numSteps >= 1);
		while(now - _lastStep >= _durations[_stepNum]) {
			ASSERT(_durations[_stepNum] > 0);
			_lastStep += _durations[_stepNum];
			_stepNum = (_stepNum + 1) % _numSteps;
		}
		updatePins();
	}

	void validate(){
		ASSERT(_stepNum < _numSteps);
		ASSERT(_numSteps <= 16 && _numSteps >= 1);

		for (uint8_t i = 0; i < _numSteps; i++){
			ASSERT(_durations[i] > 0);
		}
	}

	void setNumSteps(uint8_t n) {
		_numSteps = n;
	}

	uint8_t numSteps() {
		return _numSteps;
	}

	void setNewPhase(uint32_t startTime, uint32_t now) {
		ASSERTF(now - startTime < (100000), "now: %lu, startTime: %lu", now, startTime);
		_lastStep = startTime;
		_stepNum = 0;
		process(now);
	}

	void resetPhase(uint32_t now) {
		_lastStep = now;
		_stepNum = 0;
		updatePins();
	}

	uint16_t getDurationSoFar(uint32_t now) {
		uint16_t soFar = 0;
		for(int8_t i = 0; i < _stepNum; i++) {
			soFar += _durations[i];
		}
		soFar += (now - _lastStep);
		return soFar;
	}

	uint16_t getDurationTotal() {
		uint16_t total = 0;
		for(int8_t i = 0; i < _numSteps; i++) {
			total += _durations[i];
		}
		return total;
	}

	uint16_t(&durations())[16]{
		return _durations;
	}

	void setStepNum(uint8_t n) {
		_stepNum = n;
		updatePins();
	}

protected:
	virtual void updatePins() = 0;

	uint8_t _stepNum = 0;
	uint8_t _numSteps = 16;
private:
	uint32_t _lastStep = 0;
	uint16_t _durations[16];
};


class MuxSequencer : public Sequencer {
public:
	MuxSequencer() {
		_numSteps = 16;
	}

	virtual void updatePins() {
		setMaskedBitsTo(MUX_WRITE, MUX_ADDR, _stepNum);
	}
};


class ClockSequencer : public Sequencer {
public:
	ClockSequencer() {
		_numSteps = 2;
	}

	virtual void updatePins() {
		if(_stepNum == 0) {
			setBitsHigh(CLK_OUT_WRITE, CLK_OUT);
		}
		else {
			clearBits(CLK_OUT_WRITE, CLK_OUT);
		}
	}
};



class ParameterManager {
public:

	void init() {
		setParameters(0, 500, 1, 1, 16);
	}

	void process(uint32_t now) {
		muxSequencer.process(now);
		clockSequencer.process(now);
	}

	void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote) {
		uint16_t clk_durationSoFar = clockSequencer.getDurationSoFar(now);
		int16_t offset;
		// find nearest qtrNote beat (fwd or bkwd)
		if(clk_durationSoFar < m_qtrNote / 2) {
			offset = -clk_durationSoFar;
		}
		else {
			offset = m_qtrNote - clk_durationSoFar;
		}
		setQuarterNote_phaseOffset(now, qtrNote, offset);
	}

	void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote) {
		setQuarterNote_phaseOffset(now, qtrNote, 0);
	}

	void setNumSteps(uint32_t now, uint8_t n) {
		setParameters(now, m_qtrNote, m_num, m_denom, n);
	}

	void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
		setParameters(now, m_qtrNote, num, denom, m_numSteps);
	}

	void setParameters(uint32_t now, uint16_t qtrNote, uint8_t num, uint8_t denom, uint8_t numSteps) {
		ASSERTF(numSteps <= 16, "numsteps: %u", numSteps);

		qtrNote = roundToNearestMult<uint16_t>(qtrNote, denom);
		uint16_t dividedDuration = num * (qtrNote / denom);

		for(uint8_t i = 0; i < numSteps; i++) {
			muxSequencer.durations()[i] = dividedDuration;
		}
		muxSequencer.setNumSteps(numSteps);

		clockSequencer.durations()[0] = min(200, qtrNote / 2);
		clockSequencer.durations()[1] = qtrNote - clockSequencer.durations()[0];

		muxSequencer.resetPhase(now);
		clockSequencer.resetPhase(now);

		m_qtrNote = qtrNote;
		m_num = num;
		m_denom = denom;
		m_numSteps = numSteps;
	}

	uint8_t numSteps(){
		return m_numSteps;
	}

	void resetPhase(uint32_t now){
		muxSequencer.resetPhase(now);
		clockSequencer.resetPhase(now);
	}

private:

	void setQuarterNote_phaseOffset(uint32_t now, uint16_t qtrNote, int16_t offset) {
		//  get durationSoFar and durationTotalfor phase calculation later
		uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
		uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

		setParameters(now, qtrNote, m_num, m_denom, m_numSteps);

		// calc new phase
		uint32_t adj_durationSoFar = (mux_durationSoFar + offset) % mux_durationTotal;
		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
		uint32_t newStartTime = now - mux_newDurationSoFar;

		muxSequencer.setNewPhase(newStartTime, now);
		clockSequencer.setNewPhase(newStartTime, now);
	}

	MuxSequencer muxSequencer;
	ClockSequencer clockSequencer;

	uint16_t m_qtrNote;
	uint8_t m_num;
	uint8_t m_denom;
	uint8_t m_numSteps;

} parameterManager;



int16_t showNumStepsFor = 0;



void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote) {
	parameterManager.setQuarterNote_beatNow(now, qtrNote);
}

void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote) {
	parameterManager.setQuarterNote_keepPhase(now, qtrNote);
}

void setNumSteps(uint32_t now, uint8_t n) {
	parameterManager.setNumSteps(now, n);
	showNumStepsFor = 1000;

	setBitsHigh(MUX_WRITE, POT_nENABLE);
	clearBits(CLK_OUT_WRITE, CLK_OUT);
}

void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
	parameterManager.setDivide(now, num, denom);
}


void process_showNumSteps(uint32_t now, int8_t delta) {
	static uint16_t stepNum = 0;

	stepNum = (stepNum + 1) % parameterManager.numSteps();
	setMaskedBitsTo(MUX_WRITE, MUX_ADDR, stepNum);

	showNumStepsFor -= delta;

	if(showNumStepsFor <= 0) {
		showNumStepsFor = 0;
		parameterManager.resetPhase(now);
		clearBits(MUX_WRITE, POT_nENABLE);
		PRINT("Exit show num steps mode.\n");
	}
}


void processInnerSequencer(uint32_t now) {
	static uint32_t prevNow = 0;
	if(showNumStepsFor) {
		process_showNumSteps(now, now - prevNow);
	}
	else {
		parameterManager.process(now);
	}
	prevNow = now;
}

void initInnerSequencer() {
	setBitsHigh(MUX_DDR, POT_nENABLE | LED_nENABLE | MUX_ADDR);
	setBitsHigh(CLK_OUT_DDR, CLK_OUT);
	parameterManager.init();
}