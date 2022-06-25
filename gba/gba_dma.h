//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"
#include "gba_memory.h"

namespace gbaDMA {
void Reset();
s32 Sync(s32 limit);
bool IsSyncPending();
void OnVblank();
void OnHblank();
void OnFIFORequest(u32 fifo);
void OnCaptureRequest();
void OnCaptureEnd();
void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadIO(u32 address, t32 *data, gbaMemory::DataType width);
}
//*************************************************************************************************