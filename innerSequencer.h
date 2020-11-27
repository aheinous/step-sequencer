#pragma once
#include <stdint.h>

void setQuarterNote(uint16_t qtrNote, uint32_t now);
void setQuarterNoteAt(uint16_t qtrNote, uint32_t at, uint32_t now);
// void setQuarterNoteWithOffset(uint16_t qtrNote, int16_t offset, uint32_t now);

void processInnerSequencer(uint32_t now);

void initInnerSequencer();