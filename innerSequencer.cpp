#include "innerSequencer.h"
#include "config.h"
#include <BPUtil.h>

class Sequencer {
public:

	void process(uint32_t now) {
		ASSERT(_stepNum < _numSteps);
		ASSERT(_numSteps <= 16 && _numSteps >= 1);
		// ASSERT()
		while(now - _lastStep >= _durations[_stepNum]) {
			ASSERTDO(_durations[_stepNum] > 0,
			{
				PVAR(_durations[_stepNum]);
				PVAR(_durations[0]);
				PVAR(_durations[1]);
				PVAR(_stepNum);
				PVAR(_numSteps);
			});
			_lastStep += _durations[_stepNum];
			_stepNum = (_stepNum + 1) % _numSteps;
		}
		updatePins();
	}

	void validate(int x){
		ASSERT(_stepNum < _numSteps);
		ASSERT(_numSteps <= 16 && _numSteps >= 1);

		for (uint8_t i = 0; i < _numSteps; i++)
		{

			ASSERTDO(_durations[i] > 0,
			{
				PVAR(_durations[i]);
				PVAR(_durations[0]);
				PVAR(_durations[1]);
				PVAR(i);
				PVAR(_numSteps);
				PVAR(x);
			});

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
		// for (size_t i = 0; i < 16; i++)
		// {
		// 	ASSERT(_durations[i] > 0);
		// }

		// PRINTLN(__func__);

		return _durations;
	}

	// uint16_t *pdurations() {
	// 	return _durations;
	// }

	// void setDurations(uint16_t(*newDurations)[16]) {
	// 	ASSERT(sizeof(*newDurations) == 32);
	// 	memcpy(_durations, *newDurations, sizeof(*newDurations));
	// }

	// void printDurations() {
	// 	for(uint8_t i = 0; i < 16; i++) {
	// 		PRINT(_durations[i]);
	// 		PRINT(", ");
	// 	}
	// 	PRINTLN();
	// }

	void setStepNum(uint8_t n) {
		_stepNum = n;
		updatePins();
	}

protected:
	virtual void updatePins() = 0;
	uint8_t _stepNum = 0;
	uint8_t _numSteps = 16;
private:
	// only public for debugging
	uint32_t _lastStep = 0;
	uint16_t _durations[16];
	// bool _blitzMode = false;
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

// MuxSequencer muxSequencer;

// ClockSequencer clockSequencer;


// uint16_t _quarterNote = 0;
// uint8_t _numSteps = 16;

// uint8_t numSteps = 16;

int16_t showNumStepsFor = 0;




// void setQuarterNoteWithOffset(uint16_t qtrNote, int16_t offset, uint32_t now) {
// 	PRINTLN(__func__);
// 	PVAR(qtrNote);
// 	PVAR(offset);
// 	PVAR(now);

// 	_quarterNote = qtrNote;

// 	//// calc getDurationSoFar and getDurationTotal. these are for
// 	//// phase calculation later
// 	uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
// 	uint16_t mux_durationTotal = muxSequencer.getDurationTotal();


// 	PVAR(mux_durationSoFar);
// 	PVAR(mux_durationTotal);
// 	PVAR(muxSequencer.numSteps());

// 	///// update mux durations
// 	for(uint8_t i = 0; i < muxSequencer.numSteps(); i++) {
// 		*muxSequencer.durations()[i] = qtrNote;
// 	}

// 	//// update clk durations
// 	*clockSequencer.durations()[0] = min(200, qtrNote / 2);
// 	*clockSequencer.durations()[1] = qtrNote - *clockSequencer.durations()[0];


// 	////// set phase
// 	if(mux_durationTotal == 0) { // first time set
// 		clockSequencer.resetPhase(now);
// 		muxSequencer.resetPhase(now);
// 	}
// 	else {
// 		uint32_t adj_durationSoFar = (mux_durationSoFar + offset) % mux_durationTotal;
// 		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
// 		uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
// 		uint32_t newStartTime = now - mux_newDurationSoFar;

// 		// PVAR(adj_durationSoFar);
// 		// PVAR(mux_newDurationTotal);
// 		// PVAR(mux_newDurationSoFar);
// 		// PVAR(now);
// 		// PVAR(newStartTime);

// 		muxSequencer.setNewPhase(newStartTime, now);
// 		clockSequencer.setNewPhase(newStartTime, now);
// 	}
// }

// void setQuarterNoteAt(uint16_t qtrNote, uint32_t at, uint32_t now) {
// 	uint16_t clk_durationSoFar = clockSequencer.getDurationSoFar(now);
// 	int16_t offset;
// 	if(clk_durationSoFar < _quarterNote / 2) {
// 		offset = -clk_durationSoFar;
// 	}
// 	else {
// 		offset = _quarterNote - clk_durationSoFar;
// 	}
// 	offset += (now - at);
// 	setQuarterNoteWithOffset(qtrNote, offset, now);
// }

// void setQuarterNote(uint16_t qtrNote, uint32_t now) {
// 	setQuarterNoteWithOffset(qtrNote, 0, now);
// }


// void setDurations(uint32_t now, uint16_t qtrNote, uint16_t (*durations)[16], uint8_t numSteps){
// 	muxSequencer.setDurations(durations);
// 	muxSequencer.setNumSteps(numSteps);
// 	muxSequencer.resetPhase(now);

// 	//// update clk durations
// 	*clockSequencer.durations()[0] = min(200, qtrNote / 2);
// 	*clockSequencer.durations()[1] = qtrNote - *clockSequencer.durations()[0];
// 	clockSequencer.resetPhase(now);

// 	// _numSteps = numSteps;
// 	_quarterNote = qtrNote;




// }


class ParameterManager {
public:

	void init() {
		PRINTLN(__func__);
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
		PRINTLN(__func__);
		if(m_qtrNote == 0) {
			// not initted
			ASSERT(false);
			return;
		}
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
		PRINTLN(__func__);
		// delay(1000);
		PVAR( now);
		PVAR( qtrNote);
		PVAR( num);
		PVAR( denom);
		PVAR( numSteps);
		PRINTLN("");

		qtrNote = roundToNearestMult<uint16_t>(qtrNote, denom);
		uint16_t dividedDuration = num * (qtrNote / denom);

		PVAR(qtrNote); PVAR(dividedDuration);
		// uint16_t durations[16];
		for(uint8_t i = 0; i < numSteps; i++) {
			(muxSequencer.durations())[i] = dividedDuration;
			// PVAR(*muxSequencer.durations()[i]);
		}
		muxSequencer.setNumSteps(numSteps);

		(clockSequencer.durations())[0] = min(200, qtrNote / 2);
		(clockSequencer.durations())[1] = qtrNote - clockSequencer.durations()[0];

		// PVAR((clockSequencer.durations())[0]);
		// PVAR((clockSequencer.durations())[1]);

		// PVAR(clockSequencer.pdurations()[0]);
		// PVAR(clockSequencer.pdurations()[1]);

		// PVAR(clockSequencer.)


		// clockSequencer.validate(1);

		muxSequencer.resetPhase(now);
		clockSequencer.resetPhase(now);

		// clockSequencer.validate(2);

		m_qtrNote = qtrNote;
		m_num = num;
		m_denom = denom;
		m_numSteps = numSteps;
		PRINTLN("exit setParameters");
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
		PRINTLN(__func__);
		//// calc getDurationSoFar and getDurationTotal. these are for
		//// phase calculation later
		uint16_t mux_durationSoFar = muxSequencer.getDurationSoFar(now);
		uint16_t mux_durationTotal = muxSequencer.getDurationTotal();

		setParameters(now, qtrNote, m_num, m_denom, m_numSteps);
		// PRINTLN("-->");

		// clockSequencer.validate(3);

		// calc new phase
		uint32_t adj_durationSoFar = (mux_durationSoFar + offset) % mux_durationTotal;
		uint16_t mux_newDurationTotal = muxSequencer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(adj_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);
		uint32_t newStartTime = now - mux_newDurationSoFar;

		// clockSequencer.validate(4);

		muxSequencer.setNewPhase(newStartTime, now);
		clockSequencer.setNewPhase(newStartTime, now);

		// clockSequencer.validate(5);

		// PRINTLN("<--");
	}

	MuxSequencer muxSequencer;
	ClockSequencer clockSequencer;

	uint16_t m_qtrNote;
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

void setNumSteps(uint32_t now, uint8_t n) {
	parameterManager.setNumSteps(now, n);
	showNumStepsFor = 1000;

	setBitsHigh(MUX_WRITE, POT_nENABLE);
	clearBits(CLK_OUT_WRITE, CLK_OUT);
}

void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
	parameterManager.setDivide(now, num, denom);
}


// void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
// 	PRINTF("set divide %u / %u\n", num, denom);
// 	uint16_t newQtrNote = roundToNearestMult<uint16_t>(_quarterNote, denom);
// 	uint16_t dividedDuration = num * (newQtrNote / denom);

// 	uint16_t durations[16];

// 	for(uint8_t i = 0; i < muxSequencer.numSteps(); i++) {
// 		durations[i] = dividedDuration;
// 	}
// 	setDurations(now, newQtrNote, &durations, muxSequencer.numSteps());
// }


// void setNumSteps(uint8_t n) {
// 	PRINTF("set num steps: %u\n", n);
// 	setBitsHigh(MUX_WRITE, POT_nENABLE);
// 	showNumStepsFor = 1000;

// 	muxSequencer.setNumSteps(n);
// 	for(uint8_t i = 0; i < n; i++) {
// 		*muxSequencer.durations()[i] = _quarterNote;
// 	}
// 	clockSequencer.setStepNum(1); // keep led off; rising edge right away on restart

// 	// muxSequencer.setBlitzMode(true);
// }


void process_showNumSteps(uint32_t now, int8_t delta) {
	static uint16_t stepNum = 0;
	stepNum = (stepNum + 1) % parameterManager.numSteps();
	setMaskedBitsTo(MUX_WRITE, MUX_ADDR, stepNum);

	// muxSequencer.process(now);
	showNumStepsFor -= delta;
	if(showNumStepsFor <= 0) {
		showNumStepsFor = 0;
		// muxSequencer.setBlitzMode(false);
		// muxSequencer.resetPhase(now);
		// clockSequencer.resetPhase(now);

		parameterManager.resetPhase(now);
		clearBits(MUX_WRITE, POT_nENABLE);
		PRINT("Exit show num steps mode.\n");
		// PRINTF("now: %lu, ms.ls: %lu, cs.ls %lu, ms.stepnum: %u cs.stepnum: %u, ms.numSteps: %u\n",
		// 	now,
		// 	muxSequencer._lastStep, clockSequencer._lastStep,
		// 	muxSequencer._stepNum, clockSequencer._stepNum,
		// 	muxSequencer.numSteps());
		// muxSequencer.printDurations();
		// clockSequencer.printDurations();
	}


}


void processInnerSequencer(uint32_t now) {
	// PRINTLN("c");
	static uint32_t prevNow = 0;
	if(showNumStepsFor) {
		process_showNumSteps(now, now - prevNow);
	}
	else {
		// muxSequencer.process(now);
		// clockSequencer.process(now);
		// PRINTLN("d");
		// delay(1000);
		parameterManager.process(now);

		// PRINTLN("e");

		// static uint8_t prevCSStepNum = 1;
		// if(clockSequencer._stepNum == 0 && prevCSStepNum != 0) {
		// 	// ASSERTF(muxSequencer._lastStep == clockSequencer._lastStep,
		// 	// 		"now: %lu, ms.ls: %lu, cs.ls %lu, ms.stepnum: %u cs.stepnum: %u, ms.numSteps: %u",
		// 	// 		now,
		// 	// 		muxSequencer._lastStep, clockSequencer._lastStep,
		// 	// 		muxSequencer._stepNum, clockSequencer._stepNum,
		// 	// 		muxSequencer.numSteps());

		// 	PVAR(muxSequencer._lastStep == clockSequencer._lastStep);
		// }
		// prevCSStepNum = clockSequencer._stepNum;

	}
	prevNow = now;
}

void initInnerSequencer() {
	setBitsHigh(MUX_DDR, POT_nENABLE | LED_nENABLE | MUX_ADDR);
	setBitsHigh(CLK_OUT_DDR, CLK_OUT);
	// PRINTLN("a");
	delay(100);
	parameterManager.init();
	// PRINTLN("b");
	delay(100);
}