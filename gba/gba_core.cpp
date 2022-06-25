//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "gba_bios.h"
#include "gba_cartridge.h"
#include "gba_control.h"
#include "gba_cpu.h"
#include "gba_display.h"
#include "gba_dma.h"
#include "gba_keyinput.h"
#include "gba_memory.h"
#include "gba_sio.h"
#include "gba_sound.h"
#include "gba_timer.h"
#include "../emulator.h"

namespace gbaCore
{
volatile bool m_run = false;
volatile bool m_end = true;

void Reset()
{
    gbaControl::Reset();
    gbaDisplay::Reset();
    gbaDMA::Reset();
    gbaKeyInput::Reset();
    gbaMemory::Reset();
    gbaSIO::Reset();
    gbaSound::Reset();
    gbaTimer::Reset();
    gbaCPU::Reset();
}

s32 NextEvent()
{
    s32 ev_timer   = gbaTimer::GetNextEvent();
    s32 ev_sound   = gbaSound::GetNextEvent();
    s32 ev_display = gbaDisplay::GetNextEvent();

    s32 ret = ev_timer;
    if (ret <= 0 || ev_sound   < ret) {ret = ev_sound;}
    if (ret <= 0 || ev_display < ret) {ret = ev_display;}

    return ret;
}

void Sync(s32 ticks)
{
    if (ticks <= 0) {return;}
    gbaTimer::Sync(ticks);
    gbaSound::Sync(ticks);
    gbaDisplay::Sync(ticks);
}

void Run()
{
    s32 next = NextEvent();

    s32 t;
    s32 ticks = 0;

    switch (gbaControl::IsHalted()) {
    case gbaControl::POWERDOWN_NONE:
        do {
            t = gbaCPU::SingleStep();
            ticks += t;
        } while (ticks < next && (!gbaControl::Sync()) && !gbaDMA::IsSyncPending() && gbaControl::IsHalted() == gbaControl::POWERDOWN_NONE);
        break;
    case gbaControl::POWERDOWN_HALT:
        ticks = NextEvent();
        break;
    case gbaControl::POWERDOWN_STOP:
        gbaKeyInput::Sync();
        gbaDisplay::Sync(gbaDisplay::GetNextEvent());
        return;
    }
    
    Sync(ticks);

    ticks = 0;
    next = NextEvent();

    while (gbaDMA::IsSyncPending()) {        
        ticks += gbaDMA::Sync(next);
        if (ticks >= next) {Sync(ticks); ticks = 0; next = NextEvent();}
    }

    if (ticks > 0) {Sync(ticks);}

    if (gbaControl::Sync()) {gbaCPU::RequestInterrupt();}
}

void StartEmulation()
{
    Reset();
    if (!gbaBIOS::IsLoaded() || !gbaCartridge::IsLoaded()) {return;}
    m_run = true;
    m_end = false;
    while (m_run) {Run();}
    gbaCartridge::StoreBackup();
    gbaCartridge::Release();
    m_end = true;
}

bool IsRunning()
{
    return !m_end;
}

void StopEmulation()
{
    m_run = false;
}
}

//*************************************************************************************************