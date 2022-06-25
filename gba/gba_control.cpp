//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "../emulator.h"
#include "gba_control.h"

namespace gbaControl {
const s32 m_PHIfrequency[4]    = {0, 4194304, 8388608, 16777216};
const s32 m_firstaccess[4]     = {5, 4, 3, 9};
const s32 m_WS0secondaccess[2] = {3, 2};
const s32 m_WS1secondaccess[2] = {5, 2};
const s32 m_WS2secondaccess[2] = {9, 2};

t8   m_IME;
t16  m_IE;
t16  m_IF;
t16  m_WAITCNT;
t8   m_POSTFLG;
t8   m_u0x04000410;
t32  m_u0x04000800;

PowerDownMode m_halt;

PowerDownMode IsHalted() {return m_halt;}
void WakeUp() {m_halt = POWERDOWN_NONE;}

void RequestInterrupt(InterruptFlag irq) {if (m_halt != POWERDOWN_STOP) {m_IF.w |= irq;} else if ((irq & (IRQ_KEYPAD | IRQ_GAMEPAK | IRQ_SIO) & m_IE.w) != 0) {WakeUp();}}

bool Sync() {
    bool irqcause = (m_IE.w & m_IF.w & IRQ_ALL) != 0;
    if (irqcause) {WakeUp();}
    return irqcause && BITTEST(m_IME.b, 0);
}

s32 GetPHITerminalFrequency() {return m_PHIfrequency[SUBVAL(m_WAITCNT.w, 11, 3)];}
bool IsGamePakPrefetchEnabled() {return BITTEST(m_WAITCNT.w, 14);}

bool IsWRAMEnabled()     {return !BITTEST(m_u0x04000800.d, 0);}
bool IsWRAM256KEnabled() {return  BITTEST(m_u0x04000800.d, 5);}

void GetWRAM256KRegionWait(gbaMemory::DataType width, s32 *N_access, s32 *S_access) {*N_access = *S_access = (16 - SUBVAL(m_u0x04000800.d, 24, 0xF)) << (width >> 2);}

void GetROMRegionWait(u32 address, gbaMemory::DataType width, s32 *N_access, s32 *S_access) {
    u32 base = ALIGN(address, width);
    s32 N, S;

         if (base < 0x0A000000) {N = m_firstaccess[SUBVAL(m_WAITCNT.w, 2, 3)]; S = m_WS0secondaccess[SUBVAL(m_WAITCNT.w,  4, 1)];}
    else if (base < 0x0C000000) {N = m_firstaccess[SUBVAL(m_WAITCNT.w, 5, 3)]; S = m_WS1secondaccess[SUBVAL(m_WAITCNT.w,  7, 1)];}
    else                        {N = m_firstaccess[SUBVAL(m_WAITCNT.w, 8, 3)]; S = m_WS2secondaccess[SUBVAL(m_WAITCNT.w, 10, 1)];}
    
    if (width == gbaMemory::TYPE_WORD) {
        N += S;
        S += S;
    }

    if ((base & 0x0001FFFF) == 0) {S = N;}

    *N_access = N;
    *S_access = S;
}

void GetSRAMRegionWait(s32 *N_access, s32 *S_access) {*N_access = *S_access = m_firstaccess[SUBVAL(m_WAITCNT.w, 0, 3)];}

void WriteIE_B0(u8 byte) {m_IE.b.b0.b = byte;}
void WriteIE_B1(u8 byte) {m_IE.b.b1.b = byte & 0x3F;}

void WriteIF_B0(u8 byte) {m_IF.b.b0.b &=        ~byte;}
void WriteIF_B1(u8 byte) {m_IF.b.b1.b &= 0x3F & ~byte;}

void WriteIME(u8 byte) {m_IME.b = byte & 0x01;}

void WriteWAITCNT_B0(u8 byte) {m_WAITCNT.b.b0.b = byte;}
void WriteWAITCNT_B1(u8 byte) {m_WAITCNT.b.b1.b = byte & ~(BIT(5) | BIT(7));}

void WritePOSTFLG(u8 byte) {m_POSTFLG.b = byte & 1;}

void WriteHALTCNT(u8 byte) {m_halt = BITTEST(byte, 7) ? POWERDOWN_STOP : POWERDOWN_HALT;}

void Write0x04000800(u8 byte) {m_u0x04000800.w.w0.b.b0.b = byte & ~(BIT(4) | BIT(6) | BIT(7));}
void Write0x04000803(u8 byte) {m_u0x04000800.w.w1.b.b1.b = byte;}

void Reset() {
    m_IME.b         = 0;
    m_IE.w          = 0;
    m_IF.w          = 0;
    m_WAITCNT.w     = 0;
    m_POSTFLG.b     = 0;
    m_u0x04000410.b = 0;
    m_u0x04000800.d = 0x0D000020;
    m_halt          = POWERDOWN_NONE;
}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    if ((base & 0xFF00FFFC) == 0x04000800) {base &= 0xFF00FFFF;}
    UNPACK_IO_BYTES(data)
    BEGIN_IO_TABLE(base, width)
        IO_WRITE_CALLBACK(0x04000200, WriteIE_B0)
        IO_WRITE_CALLBACK(0x04000201, WriteIE_B1)
        IO_WRITE_CALLBACK(0x04000202, WriteIF_B0)
        IO_WRITE_CALLBACK(0x04000203, WriteIF_B1)
        IO_WRITE_CALLBACK(0x04000208, WriteIME)
        IO_WRITE_CALLBACK(0x04000204, WriteWAITCNT_B0)
        IO_WRITE_CALLBACK(0x04000205, WriteWAITCNT_B1)
        IO_WRITE_CALLBACK(0x04000300, WritePOSTFLG)
        IO_WRITE_CALLBACK(0x04000301, WriteHALTCNT)
        IO_WRITE_DIRECT(0x04000410, m_u0x04000410.b)
        IO_WRITE_CALLBACK(0x04000800, Write0x04000800)
        IO_WRITE_CALLBACK(0x04000803, Write0x04000803)
    END_IO_TABLE()
}

void ReadIO(u32 address, t32 *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    if ((base & 0xFF00FFFC) == 0x04000800) {base &= 0xFF00FFFF;}
    UNPACK_IO_POINTERS(data)
    BEGIN_IO_TABLE(base, width)
        IO_READ_DIRECT(0x04000200, m_IE.b.b0.b)
        IO_READ_DIRECT(0x04000201, m_IE.b.b1.b)
        IO_READ_DIRECT(0x04000202, m_IF.b.b0.b)
        IO_READ_DIRECT(0x04000203, m_IF.b.b1.b)
        IO_READ_DIRECT(0x04000204, m_WAITCNT.b.b0.b)
        IO_READ_DIRECT(0x04000205, m_WAITCNT.b.b1.b)
        IO_READ_DIRECT(0x04000206, 0)
        IO_READ_DIRECT(0x04000207, 0)
        IO_READ_DIRECT(0x04000208, m_IME.b)
        IO_READ_DIRECT(0x04000209, 0)
        IO_READ_DIRECT(0x0400020A, 0)
        IO_READ_DIRECT(0x0400020B, 0)
        IO_READ_DIRECT(0x04000300, m_POSTFLG.b)
        IO_READ_DIRECT(0x04000301, 0)
        IO_READ_DIRECT(0x04000302, 0)
        IO_READ_DIRECT(0x04000303, 0)
        IO_READ_DIRECT(0x04000410, m_u0x04000410.b)
        IO_READ_DIRECT(0x04000800, m_u0x04000800.w.w0.b.b0.b)
        IO_READ_DIRECT(0x04000801, 0)
        IO_READ_DIRECT(0x04000802, 0)
        IO_READ_DIRECT(0x04000803, m_u0x04000800.w.w1.b.b1.b)
    END_IO_TABLE()
}
}
//*************************************************************************************************