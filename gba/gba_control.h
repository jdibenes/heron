//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"
#include "gba_memory.h"

namespace gbaControl {
enum InterruptFlag {
    IRQ_VBLANK   = BIT(0),
    IRQ_HBLANK   = BIT(1),
    IRQ_VCOUNTER = BIT(2),
    IRQ_TIMER0   = BIT(3),
    IRQ_TIMER1   = BIT(4),
    IRQ_TIMER2   = BIT(5),
    IRQ_TIMER3   = BIT(6),
    IRQ_SIO      = BIT(7),
    IRQ_DMA0     = BIT(8),
    IRQ_DMA1     = BIT(9),
    IRQ_DMA2     = BIT(10),
    IRQ_DMA3     = BIT(11),
    IRQ_KEYPAD   = BIT(12),
    IRQ_GAMEPAK  = BIT(13),
    IRQ_ALL      = BIT(14) - 1
};

enum PowerDownMode {
    POWERDOWN_NONE,
    POWERDOWN_HALT,
    POWERDOWN_STOP
};

void Reset();
PowerDownMode IsHalted();
void RequestInterrupt(InterruptFlag irq);
bool Sync();
s32 GetPHITerminalFrequency();
bool IsGamePakPrefetchEnabled();
bool IsWRAMEnabled();
bool IsWRAM256KEnabled();
void GetWRAM256KRegionWait(gbaMemory::DataType width, s32 *N_access, s32 *S_access);
void GetROMRegionWait(u32 address, gbaMemory::DataType width, s32 *N_access, s32 *S_access);
void GetSRAMRegionWait(s32 *N_access, s32 *S_access);
void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadIO(u32 address, t32 *data, gbaMemory::DataType width);
}
//*************************************************************************************************