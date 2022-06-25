//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"

namespace gbaMemory {
enum DataType {
    TYPE_BYTE     = 1,
    TYPE_HALFWORD = 2,
    TYPE_WORD     = 4
};

void Reset();
void Write(u32 address, t32 const *data, gbaMemory::DataType width, s32 *N_access, s32 *S_access);
void Read(u32 address, t32 *data, gbaMemory::DataType width, s32 *N_access, s32 *S_access);
}
//*************************************************************************************************