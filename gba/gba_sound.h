//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once
#include "../types.h"
#include "gba_control.h"
#include "gba_memory.h"

#define GBA_SAMPLERATE 32768

namespace gbaSound
{
void Sync(s32 ticks);
void Reset();
s32 GetNextEvent();
void OnTimerOverflow(gbaControl::InterruptFlag timer);
void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadIO(u32 address, t32 *data, gbaMemory::DataType width);
}
