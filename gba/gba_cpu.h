//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"

namespace gbaCPU
{
bool IsOpcodeFetch(t32 const *data);
s32 Reset();
s32 SingleStep();
s32 RequestInterrupt();
u32 GetPrefetch();
}

//*************************************************************************************************