#pragma once
#include <stdint.h>

// void setQuarterNote(uint16_t qtrNote, uint32_t now);
// void setQuarterNoteAt(uint16_t qtrNote, uint32_t at, uint32_t now);
// void setQuarterNoteWithOffset(uint16_t qtrNote, int16_t offset, uint32_t now);

void processInnerSequencer(uint32_t now);

// void setNumSteps(uint8_t n);

// void setDivide(uint32_t now, uint8_t num, uint8_t denom);

void initInnerSequencer();


void setQuarterNote_beatNow(uint32_t now, uint16_t qtrNote);

void setQuarterNote_keepPhase(uint32_t now, uint16_t qtrNote);

void setNumSteps(uint32_t now, uint8_t n);

void setDivide(uint32_t now, uint8_t num, uint8_t denom);

