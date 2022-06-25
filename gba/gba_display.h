//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once
#include "../types.h"
#include "gba_memory.h"

#define GBA_SCREENWIDTH  240
#define GBA_SCREENHEIGHT 160

namespace gbaDisplay
{
void Sync(s32 ticks);
void Reset();
void WritePaletteRAM(u32 address, t32 const *data, gbaMemory::DataType width);
void WriteVRAM(u32 address, t32 const *data, gbaMemory::DataType width);
void WriteOAM(u32 address, t32 const *data, gbaMemory::DataType width);
void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadPaletteRAM(u32 address, t32 *data, gbaMemory::DataType width);
void ReadVRAM(u32 address, t32 *data, gbaMemory::DataType width);
void ReadOAM(u32 address, t32 *data, gbaMemory::DataType width);
void ReadIO(u32 address, t32 *data, gbaMemory::DataType width);
s32 GetNextEvent();
}
