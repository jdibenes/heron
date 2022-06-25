//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"
#include "gba_memory.h"

namespace gbaTimer {
void Reset();
void Sync(s32 ticks);
s32 GetNextEvent();
void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadIO(u32 address, t32 *data, gbaMemory::DataType width);
}
//*************************************************************************************************