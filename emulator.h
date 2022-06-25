//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "types.h"
#include "macros.h"

#include "gba/gba_bios.h"
#include "gba/gba_cartridge.h"
#include "gba/gba_core.h"
#include "gba/gba_display.h"
#include "gba/gba_keyinput.h"
#include "gba/gba_sound.h"

namespace Emulator
{
void LogMessage(char const *format, ...);
u16 ReadKeypad();
void SendSoundSample(s32 so1, s32 so2);
void SendVideoFrame(u16 frame[GBA_SCREENHEIGHT][GBA_SCREENWIDTH]);
}
