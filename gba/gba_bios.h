//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"
#include "gba_memory.h"

namespace gbaBIOS {
bool Load(char const *filename);
bool IsLoaded();
void Read(u32 address, t32 *data, gbaMemory::DataType width);
}
//*************************************************************************************************