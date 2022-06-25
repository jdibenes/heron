//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include <cstring>
#include "../emulator.h"
#include "gba_cpu.h"
#include "gba_control.h"
#include "gba_bios.h"
#include "gba_cartridge.h"
#include "gba_display.h"
#include "gba_sound.h"
#include "gba_dma.h"
#include "gba_timer.h"
#include "gba_sio.h"
#include "gba_keyinput.h"
#include "gba_memory.h"

namespace gbaMemory {
u8 m_WRAM256K[0x40000];
u8 m_WRAM32K[0x8000];

void Reset() {
    memset(m_WRAM256K, 0, sizeof(m_WRAM256K));
    memset(m_WRAM32K,  0, sizeof(m_WRAM32K));
}

void Write(u32 address, t32 const *data, DataType width, s32 *N_access, s32 *S_access) {
    u32 base = ALIGN(address, width);

    switch (SUBVAL(base, 24, 0xFF)) {
    case 0x00:
    case 0x01:
_WRITE_READONLY:
        *N_access = *S_access = 1;
        break;
    case 0x02:
        if (!gbaControl::IsWRAMEnabled())     {goto _WRITE_READONLY;}
        if (!gbaControl::IsWRAM256KEnabled()) {goto _WRITE_WRAM32K;}
        base &= sizeof(m_WRAM256K) - 1;
        WRITE(m_WRAM256K, base, data, width);
        gbaControl::GetWRAM256KRegionWait(width, N_access, S_access);
        break;
    case 0x03:
        if (!gbaControl::IsWRAMEnabled())     {goto _WRITE_READONLY;}
_WRITE_WRAM32K:
        base &= sizeof(m_WRAM32K) - 1;
        WRITE(m_WRAM32K, base, data, width);
        *N_access = *S_access = 1;
        break;
    case 0x04:
             if (base < 0x04000060) {gbaDisplay::WriteIO(base, data, width);}
        else if (base < 0x040000B0) {gbaSound::WriteIO(base, data, width);}
        else if (base < 0x04000100) {gbaDMA::WriteIO(base, data, width);}
        else if (base < 0x04000120) {gbaTimer::WriteIO(base, data, width);}
        else if (base < 0x04000130) {gbaSIO::WriteIO(base, data, width);}
        else if (base < 0x04000134) {gbaKeyInput::WriteIO(base, data, width);}
        else if (base < 0x04000200) {gbaSIO::WriteIO(base, data, width);}
        else                        {gbaControl::WriteIO(base, data, width);}
        *N_access = *S_access = 1;
        break;
    case 0x05:
        gbaDisplay::WritePaletteRAM(base, data, width);
        *N_access = *S_access = (width >> 2) + 1;
        break;
    case 0x06:
        gbaDisplay::WriteVRAM(base, data, width);
        *N_access = *S_access = (width >> 2) + 1;
        break;
    case 0x07:
        gbaDisplay::WriteOAM(base, data, width);
        *N_access = *S_access = 1;
        break;
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        gbaCartridge::WriteROMRegion(base, data, width);
        gbaControl::GetROMRegionWait(base, width, N_access, S_access);
        break;
    case 0x0E:
    case 0x0F:
        gbaCartridge::WriteSRAMRegion(address, (data->d >> ((address & (width - 1)) << 3)) & 0xFF);
        gbaControl::GetSRAMRegionWait(N_access, S_access);
        break;
    default: 
        goto _WRITE_READONLY;
    }
}

void ReadCPUPrefetch(u32 address, t32 *data, DataType width) {
    t32 prefetch;

    prefetch.d = gbaCPU::GetPrefetch();

    switch (width) {
    case TYPE_WORD:     data->d           =  prefetch.d;                                   break;
    case TYPE_HALFWORD: data->w.w0.w      = (prefetch.d >> ((address & 2) << 3)) & 0xFFFF; break;
    case TYPE_BYTE:     data->w.w0.b.b0.b = (prefetch.d >> ((address & 3) << 3)) & 0xFF;   break;
    }
}

void Read(u32 address, t32 *data, DataType width, s32 *N_access, s32 *S_access) {
    u32 base = ALIGN(address, width);
    u8  byte;

    data->d = 0;

    switch (SUBVAL(base, 24, 0xFF)) {
    case 0x00:
        ReadCPUPrefetch(base, data, width);
        gbaBIOS::Read(base, data, width);
        *N_access = *S_access = 1;
        break;
    case 0x01:
_READ_PREFETCH:
        ReadCPUPrefetch(base, data, width);
        *N_access = *S_access = 1;
        break;
    case 0x02:
        if (!gbaControl::IsWRAMEnabled())     {goto _READ_PREFETCH;}
        if (!gbaControl::IsWRAM256KEnabled()) {goto _READ_WRAM32K;}
        base &= sizeof(m_WRAM256K) - 1;
        READ(m_WRAM256K, base, data, width);
        gbaControl::GetWRAM256KRegionWait(width, N_access, S_access);
        break;
    case 0x03:
        if (!gbaControl::IsWRAMEnabled())     {goto _READ_PREFETCH;}
_READ_WRAM32K:
        base &= sizeof(m_WRAM32K) - 1;
        READ(m_WRAM32K, base, data, width);
        *N_access = *S_access = 1;
        break;
    case 0x04:
        ReadCPUPrefetch(base, data, width);
             if (base < 0x04000060) {gbaDisplay::ReadIO(base, data, width);}
        else if (base < 0x040000B0) {gbaSound::ReadIO(base, data, width);}
        else if (base < 0x04000100) {gbaDMA::ReadIO(base, data, width);}
        else if (base < 0x04000120) {gbaTimer::ReadIO(base, data, width);}
        else if (base < 0x04000130) {gbaSIO::ReadIO(base, data, width);}
        else if (base < 0x04000134) {gbaKeyInput::ReadIO(base, data, width);}
        else if (base < 0x04000200) {gbaSIO::ReadIO(base, data, width);}
        else                        {gbaControl::ReadIO(base, data, width);}
        *N_access = *S_access = 1;
        break;
    case 0x05:
        gbaDisplay::ReadPaletteRAM(base, data, width);
        *N_access = *S_access = (width >> 2) + 1;
        break;
    case 0x06:
        gbaDisplay::ReadVRAM(base, data, width);
        *N_access = *S_access = (width >> 2) + 1;
        break;
    case 0x07:
        gbaDisplay::ReadOAM(base, data, width);
        *N_access = *S_access = 1;
        break;
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        gbaCartridge::ReadROMRegion(base, data, width);
        if (gbaControl::IsGamePakPrefetchEnabled() && gbaCPU::IsOpcodeFetch(data)) {
            *N_access = *S_access = (width >> 2) + 1;
        }
        else {
            gbaControl::GetROMRegionWait(base, width, N_access, S_access);
        }
        break;
    case 0x0E:
    case 0x0F:
        byte = gbaCartridge::ReadSRAMRegion(address);
        switch (width) {
        case TYPE_WORD:     data->w.w1.b.b1.b = byte;
                            data->w.w1.b.b0.b = byte;
        case TYPE_HALFWORD: data->w.w0.b.b1.b = byte;
        case TYPE_BYTE:     data->w.w0.b.b0.b = byte;
        }
        gbaControl::GetSRAMRegionWait(N_access, S_access);
        break;
    default:
        goto _READ_PREFETCH;
    }
}
}
//*************************************************************************************************