#pragma once
#include <stdint.h>
#include "config.h"

void initSequencePlayers();
void processSequencePlayers(uint32_t now);

void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote);
void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote);
void setNumSteps(uint32_t now, uint8_t n);
void setDivide(uint32_t now, uint8_t num, uint8_t denom);
void setRhythm(uint32_t now, const uint16_t(*rhythm)[MAX_STEPS]);
void resetPhase(uint32_t now);

void setSingleStepMode(bool enabled);
bool inSingleStepMode();
void singleStep();
