#include "innerSequencer.h"
#include "config.h"
#include <BPUtil.h>
#include <limits.h>

class Sequencer {
public:

	void process(uint32_t now) {
		ASSERT(_stepNum < _numSteps);
		ASSERT(_numSteps <= MAX_STEPS && _numSteps >= 1);
		while(now - _lastStep >= _durations[_stepNum]) {
			ASSERT(_durations[_stepNum] > 0);
			_lastStep += _durations[_stepNum];
			_stepNum = (_stepNum + 1) % _numSteps;
		}
	}

	void validate(){
		ASSERT(_stepNum < _numSteps);
		ASSERT(_numSteps <= MAX_STEPS && _numSteps >= 1);
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

	void moveTime(int16_t delta, uint32_t now){
		uint32_t newStartTime = now - getDurationSoFar(now) - delta;
		uint16_t durTotal = getDurationTotal();
		while(newStartTime - now < UINT32_MAX/2 ){ // newStartTime > now, but account for possible overflow
			newStartTime -= durTotal;
		}

		_lastStep = newStartTime;

		_stepNum = 0;
		process(now);
	}

	void resetPhase(uint32_t now) {
		_lastStep = now;
		_stepNum = 0;
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

	uint16_t(&durations())[MAX_STEPS]{
		return _durations;
	}


	void printDurations(){
		PRINT("n steps: ");
		PRINT(_numSteps);
		PRINT(" durs: ");
		for(uint8_t i = 0; i < MAX_STEPS; i++){
			PRINT(_durations[i]);
			PRINT(" ");
		}
		PRINTLN("");
	}

	void setStepNum(uint8_t n) {
		_stepNum = n;
	}

	uint8_t stepNum() {
		return _stepNum;
	}

protected:
	virtual void updatePins() = 0;

	uint8_t _stepNum = 0;
	uint8_t _numSteps = MAX_STEPS;
private:
	uint32_t _lastStep = 0;
	uint16_t _durations[MAX_STEPS];
};


class MuxSequencer : public Sequencer {
public:
	MuxSequencer() {
		_numSteps = MAX_STEPS;
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

// TODO weird initial rhythm values

class ParameterManager {
public:

	void init() {
		setParameters(0, 500, 1, 1, MAX_STEPS, NULL);
	}

	void process(uint32_t now) {
		// uint8_t muxStartStep = muxSequencer.stepNum();
		// uint8_t clkStartStep = clockSequencer.stepNum();

		// ASSERT(false);
		muxSequencer.process(now);
		clockSequencer.process(now);

		muxSequencer.updatePins();
		clockSequencer.updatePins();


		// check syncronization
		// if(muxSequencer.stepNum() == 0 && muxStartStep != 0){
		// 	if(clkStartStep == 0 || clockSequencer.stepNum() != 0){
		// 		PRINTLN("OUT OF STEP");
		// 		PVAR(clkStartStep);
		// 		PVAR(clockSequencer.stepNum());
		// 		PVAR(clockSequencer.getDurationSoFar(now));
		// 		PVAR(clockSequencer.getDurationTotal());
		// 		PVAR(muxSequencer.getDurationTotal());
		// 		PRINTLN("");
		// 		// ASSERT(false);
		// 	}
		// 	else{
		// 		PRINTLN("IN STEP");
		// 	}
		// }

		// ASSERT(false);
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

		muxSequencer.moveTime(offset, now);
		// clockSequencer.moveTime(offset, now);

		// setQuarterNote_phaseOffset(now, qtrNote, offset);
		// setQuarterNote_phaseOffset(now, qtrNote, 0);
		setQuarterNote_keepPhase(now, qtrNote);
	}

	// void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote) {
	// 	setQuarterNote_phaseOffset(now, qtrNote, 0);
	// }

	void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote){
		PVAR(qtrNote);
		//  get durationSoFar and durationTotalfor phase calculation later
		uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
		uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

		setParameters(now, qtrNote, m_num, m_denom, m_numSteps, m_rhythm);


		// ASSERT(muxSequencer.durations()[0] == 0);
		// ASSERT(false);

		// calc new phase
		uint32_t adj_durationSoFar = (mux_durationSoFar) % mux_durationTotal;
		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
		// uint32_t newStartTime = now - mux_newDurationSoFar;



		muxSequencer.moveTime(mux_newDurationSoFar, now);
		// ASSERT(false);
		clockSequencer.moveTime(mux_newDurationSoFar, now);
		// ASSERT(false);


		// muxSequencer.setNewPhase(newStartTime, now);
		// clockSequencer.setNewPhase(newStartTime, now);
	}
	void setNumSteps(uint32_t now, uint8_t n) {
		uint8_t stepNum = muxSequencer.stepNum();
		setParameters(now, m_qtrNote, m_num, m_denom, n, m_rhythm);
		muxSequencer.setStepNum(stepNum < n ? stepNum : 0);
	}

	void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
		setParameters(now, m_qtrNote, num, denom, m_numSteps, m_rhythm);
	}

	void setRhythm(uint32_t now, const uint16_t (*rhythm)[MAX_STEPS]){
		setParameters(now, m_qtrNote, m_num, m_denom, m_numSteps, rhythm);
	}




	uint8_t numSteps() {
		return m_numSteps;
	}

	void resetPhase(uint32_t now) {
		muxSequencer.resetPhase(now);
		// muxSequencer.updatePins();
		clockSequencer.resetPhase(now);
	}

	void singleStep() {
		muxSequencer.setStepNum((muxSequencer.stepNum() + 1) % muxSequencer.numSteps());
		muxSequencer.updatePins();
	}

	void updateMuxPins() {
		muxSequencer.updatePins();
	}

private:

	void setParameters(uint32_t now,
						uint16_t qtrNote,
						uint8_t num,
						uint8_t denom,
						uint8_t numSteps,
						const uint16_t(*rhythm)[MAX_STEPS]) {
		ASSERTF(numSteps <= MAX_STEPS, "numsteps: %u", numSteps);

		qtrNote = roundToNearestMult<uint16_t>(qtrNote, denom);
		uint16_t dividedDuration = num * (qtrNote / denom);

		muxSequencer.setNumSteps(numSteps);
		setMuxRhythmDurations(dividedDuration, rhythm);

		clockSequencer.durations()[0] = min(200, qtrNote / 2);
		clockSequencer.durations()[1] = qtrNote - clockSequencer.durations()[0];

		muxSequencer.resetPhase(now);
		clockSequencer.resetPhase(now);

		m_qtrNote = qtrNote;
		m_num = num;
		m_denom = denom;
		m_numSteps = numSteps;
		m_rhythm = rhythm;
	}


	void setMuxRhythmDurations(uint16_t base, const uint16_t(*rhythm)[MAX_STEPS]) {
		// TODO funny pause
		uint16_t totalDuration = base * m_numSteps;
		uint16_t durationThusFar = 0;

		for(uint8_t i = 0; i < m_numSteps; i++) {
			PVAR((*rhythm)[i] );
			uint16_t scale = (rhythm==NULL) ? 1000 : (*rhythm)[i];
			muxSequencer.durations()[i] = ((uint32_t)base) * scale / 1000;
			durationThusFar += muxSequencer.durations()[i];
		}

		int32_t fudge = totalDuration - durationThusFar;
		PVAR(durationThusFar);
		PVAR(totalDuration);
		PVAR(base);
		PVAR(fudge);
		muxSequencer.durations()[m_numSteps - 1] += fudge;
		muxSequencer.printDurations();
	//	ASSERT(muxSequencer.durations()[0] == 0);
		// ASSERT(fudge < (int32_t) muxSequencer.durations()[numSteps - 1]);

	}

	// void setQuarterNote_phaseOffset(uint32_t now, uint16_t qtrNote, int16_t offset) {
	// 	PVAR(qtrNote);
	// 	//  get durationSoFar and durationTotalfor phase calculation later
	// 	uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
	// 	uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

	// 	setParameters(now, qtrNote, m_num, m_denom, m_numSteps, m_rhythm);

	// 	// calc new phase
	// 	uint32_t adj_durationSoFar = (mux_durationSoFar + offset) % mux_durationTotal;
	// 	uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
	// 	uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
	// 	uint32_t newStartTime = now - mux_newDurationSoFar;

	// 	muxSequencer.setNewPhase(newStartTime, now);
	// 	clockSequencer.setNewPhase(newStartTime, now);
	// }

	MuxSequencer muxSequencer;
	ClockSequencer clockSequencer;

	uint16_t m_qtrNote;
	const uint16_t(*m_rhythm)[MAX_STEPS] = NULL;
	uint8_t m_num;
	uint8_t m_denom;
	uint8_t m_numSteps;

} parameterManager;



void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote) {
	parameterManager.setQuarterNote_beatNow(now, qtrNote);
}

void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote) {
	parameterManager.setQuarterNote_keepPhase(now, qtrNote);
}

void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
	parameterManager.setDivide(now, num, denom);
}

void setRhythm(uint32_t now, const uint16_t (*rhythm)[MAX_STEPS]){
	parameterManager.setRhythm(now, rhythm);
}

void resetPhase(uint32_t now) {
	parameterManager.resetPhase(now);
}


/////////////// single step mode //////////

bool _inSingleStepMode = false;

void setSingleStepMode(bool ssmode) {
	if(_inSingleStepMode == ssmode) {
		return;
	}
	_inSingleStepMode = ssmode;
	if(ssmode) {
		clearBits(CLK_OUT_WRITE, CLK_OUT);
	}

}
bool inSingleStepMode() {
	return _inSingleStepMode;
}
void singleStep() {
	ASSERT(_inSingleStepMode);
	parameterManager.singleStep();
}


/////////////// show num steps //////////////

int16_t showNumStepsFor = 0;

void setNumSteps(uint32_t now, uint8_t n) {
	parameterManager.setNumSteps(now, n);
	showNumStepsFor = 1000;

	setBitsHigh(MUX_WRITE, POT_nENABLE);
	clearBits(CLK_OUT_WRITE, CLK_OUT);
}


void process_showNumSteps(uint32_t now, int8_t delta) {
	static uint16_t stepNum = 0;

	stepNum = (stepNum + 1) % parameterManager.numSteps();
	setMaskedBitsTo(MUX_WRITE, MUX_ADDR, stepNum);

	showNumStepsFor -= delta;

	if(showNumStepsFor <= 0) {
		showNumStepsFor = 0;
		clearBits(MUX_WRITE, POT_nENABLE);
		if(_inSingleStepMode) {
			parameterManager.updateMuxPins();
		}
		else {
			parameterManager.resetPhase(now);
			// parameterManager.stepNum();
		}
		PRINT("Exit show num steps mode.\n");
	}
}


void processInnerSequencer(uint32_t now) {
	static uint32_t prevNow = 0;
	if(showNumStepsFor) {
		process_showNumSteps(now, now - prevNow);
	}
	else if(_inSingleStepMode) {
		// ...
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