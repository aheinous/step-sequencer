#include "tapHandler.h"
#include <BPUtil.h>
#include "config.h"
#include "Debouncer.h"
#include "sequencePlayers.h"


Debouncer tapDebouncer;



template <typename T, uint8_t SIZE>
class LastNBuffer{
public:
	LastNBuffer(T init){
		for(uint8_t i=0; i<SIZE; i++){
			data[i] = init;
		}
	}

	inline void push(T e){
		head = (head+1) % SIZE;
		data[head] = e;
	}

	inline T peek(uint8_t offset){
		ASSERT(offset < SIZE);
		return data[(head - offset + SIZE) % SIZE];
	}

	inline uint8_t size(){
		return SIZE;
	}
private:
	T data[SIZE];
	uint8_t head = 0;
};


LastNBuffer<uint32_t, 4> tapHistory(-MAX_QUARTER_NOTE);

static bool withinPercent(int32_t x, int32_t y, int8_t percent){
	int32_t percentOff = (abs(x-y)*100) / x;
	return percentOff <= percent;
}

static void onTap(uint32_t now){
	tapHistory.push(now);


	uint16_t firstDelta = now-tapHistory.peek(1);
	if(firstDelta > MAX_QUARTER_NOTE){
		return;
	}
	if(firstDelta < MIN_QUARTER_NOTE){
		PRINTLN("super quick press");
	}

	uint8_t intvalCnt = 1;

	for(; intvalCnt<tapHistory.size()-1; intvalCnt++){
		uint16_t curDelta = tapHistory.peek(intvalCnt)-tapHistory.peek(intvalCnt+1);
		if(!withinPercent(firstDelta, curDelta, 10)){
			break;
		}
	}

	uint16_t qtrNote = (now - tapHistory.peek(intvalCnt)) / (intvalCnt);
	setQuarterNote_beatNow(now, qtrNote);
}


void processTap(uint32_t now){
	tapDebouncer.update(isBitHigh(nTAP_READ, nTAP), now);

	if(tapDebouncer.justPressed()){
		onTap(now);
	}
}

void initTapHandler(){
	setBitsHigh(nTAP_WRITE, nTAP);
}