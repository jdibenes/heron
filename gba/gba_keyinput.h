//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"
#include "gba_memory.h"

namespace gbaKeyInput {
enum KeypadButton {
    BUTTON_A      = BIT(0),
    BUTTON_B      = BIT(1),
    BUTTON_SELECT = BIT(2),
    BUTTON_START  = BIT(3),
    BUTTON_RIGHT  = BIT(4),
    BUTTON_LEFT   = BIT(5),
    BUTTON_UP     = BIT(6),
    BUTTON_DOWN   = BIT(7),
    BUTTON_R      = BIT(8),
    BUTTON_L      = BIT(9),
    BUTTON_ALL    = BIT(10) - 1
};

void Reset();
void Sync();
void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadIO(u32 address, t32 *data, gbaMemory::DataType width);
}
//*************************************************************************************************