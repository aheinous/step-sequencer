#include "sequencePlayers.h"
#include "config.h"
#include <BPUtil.h>
#include <limits.h>

class SequencePlayer {
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


	void moveTime(uint32_t now, int16_t delta) {
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


class MuxSequencePlayer : public SequencePlayer {
public:
	MuxSequencePlayer() {
		_numSteps = MAX_STEPS;
	}

	virtual void updatePins() {
		setMaskedBitsTo(MUX_WRITE, MUX_ADDR, _stepNum);
	}
};


class ClkSequencePlayer : public SequencePlayer {
public:
	ClkSequencePlayer() {
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

class PlayerManager {
public:

	void init() {
		setParameters(0, 500, 1, 1, MAX_STEPS, NULL);
	}

	void process(uint32_t now) {
		muxSequencePlayer.process(now);
		clkSequencePlayer.process(now);

		muxSequencePlayer.updatePins();
		clkSequencePlayer.updatePins();


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
	}

	void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote) {
		uint16_t clk_durationSoFar = clkSequencePlayer.getDurationSoFar(now);
		int16_t offset;
		// find nearest qtrNote beat (fwd or bkwd)
		if(clk_durationSoFar < m_qtrNote / 2) {
			offset = -clk_durationSoFar;
		}
		else {
			offset = m_qtrNote - clk_durationSoFar;
		}

		muxSequencePlayer.moveTime(now, offset);
		setQuarterNote_keepPhase(now, qtrNote);
	}


	void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote){
		PVAR(qtrNote);
		//  get durationSoFar and durationTotalfor phase calculation later
		uint16_t mux_durationSoFar = muxSequencePlayer.getDurationSoFar(now);
		uint16_t mux_durationTotal = muxSequencePlayer.getDurationTotal();

		setParameters(now, qtrNote, m_num, m_denom, m_numSteps, m_rhythm);

		// calc new phase
		uint16_t mux_newDurationTotal = muxSequencePlayer.getDurationTotal();
		uint16_t mux_newDurationSoFar = map(mux_durationSoFar, 0, mux_durationTotal, 0, mux_newDurationTotal);

		muxSequencePlayer.moveTime(now, mux_newDurationSoFar);
		clkSequencePlayer.moveTime(now, mux_newDurationSoFar);
	}
	void setNumSteps(uint32_t now, uint8_t numSteps) {
		setParameters(now, m_qtrNote, m_num, m_denom, numSteps, m_rhythm);
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
		muxSequencePlayer.resetPhase(now);
		clkSequencePlayer.resetPhase(now);
	}

	void singleStep() {
		muxSequencePlayer.setStepNum((muxSequencePlayer.stepNum() + 1) % muxSequencePlayer.numSteps());
		muxSequencePlayer.updatePins();
	}

	void updateMuxPins() {
		muxSequencePlayer.updatePins();
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

		muxSequencePlayer.setNumSteps(numSteps);
		setMuxRhythmDurations(dividedDuration, rhythm);

		clkSequencePlayer.durations()[0] = min(200, qtrNote / 2);
		clkSequencePlayer.durations()[1] = qtrNote - clkSequencePlayer.durations()[0];

		muxSequencePlayer.resetPhase(now);
		clkSequencePlayer.resetPhase(now);

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
			uint16_t scale = (rhythm==NULL) ? 1000 : (*rhythm)[i];
			muxSequencePlayer.durations()[i] = ((uint32_t)base) * scale / 1000;
			durationThusFar += muxSequencePlayer.durations()[i];
		}

		int32_t fudge = totalDuration - durationThusFar;

		muxSequencePlayer.durations()[m_numSteps - 1] += fudge;
		muxSequencePlayer.printDurations();
	}


	MuxSequencePlayer muxSequencePlayer;
	ClkSequencePlayer clkSequencePlayer;

	uint16_t m_qtrNote;
	const uint16_t(*m_rhythm)[MAX_STEPS] = NULL;
	uint8_t m_num;
	uint8_t m_denom;
	uint8_t m_numSteps;

} playerManager;



void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote) {
	playerManager.setQuarterNote_beatNow(now, qtrNote);
}

void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote) {
	playerManager.setQuarterNote_keepPhase(now, qtrNote);
}

void setDivide(uint32_t now, uint8_t num, uint8_t denom) {
	playerManager.setDivide(now, num, denom);
}

void setRhythm(uint32_t now, const uint16_t (*rhythm)[MAX_STEPS]){
	playerManager.setRhythm(now, rhythm);
}

void resetPhase(uint32_t now) {
	playerManager.resetPhase(now);
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
	playerManager.singleStep();
}


/////////////// show num steps //////////////

int16_t showNumStepsFor = 0;

void setNumSteps(uint32_t now, uint8_t n) {
	playerManager.setNumSteps(now, n);
	showNumStepsFor = 1000;

	setBitsHigh(MUX_WRITE, POT_nENABLE);
	clearBits(CLK_OUT_WRITE, CLK_OUT);
}


void process_showNumSteps(uint32_t now, int8_t delta) {
	static uint16_t stepNum = 0;

	stepNum = (stepNum + 1) % playerManager.numSteps();
	setMaskedBitsTo(MUX_WRITE, MUX_ADDR, stepNum);

	showNumStepsFor -= delta;

	if(showNumStepsFor <= 0) {
		showNumStepsFor = 0;
		clearBits(MUX_WRITE, POT_nENABLE);
		if(_inSingleStepMode) {
			playerManager.updateMuxPins();
		}
		else {
			playerManager.resetPhase(now);
		}
		PRINT("Exit show num steps mode.\n");
	}
}


void processSequencePlayers(uint32_t now) {
	static uint32_t prevNow = 0;
	if(showNumStepsFor > 0) {
		process_showNumSteps(now, now - prevNow);
	}
	else if(!_inSingleStepMode) {
		playerManager.process(now);
	}
	prevNow = now;
}

void initSequencePlayers() {
	setBitsHigh(MUX_DDR, POT_nENABLE | LED_nENABLE | MUX_ADDR);
	setBitsHigh(CLK_OUT_DDR, CLK_OUT);
	playerManager.init();
}