//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "../emulator.h"
#include "gba_control.h"
#include "gba_keyinput.h"

namespace gbaKeyInput {
t16  m_KEYINPUT;
t16  m_KEYCNT;

void Reset() {
    m_KEYINPUT.w = BUTTON_ALL;
    m_KEYCNT.w   = 0;
}

void Sync() {
    u16 irqbits = m_KEYCNT.w & BUTTON_ALL;
    m_KEYINPUT.w = Emulator::ReadKeypad() & BUTTON_ALL;
    if ((!BITTEST(m_KEYCNT.w, 14) && gbaControl::IsHalted() != gbaControl::POWERDOWN_STOP) || (irqbits == 0)) {return;}
    u16 irqcause = ~m_KEYINPUT.w & irqbits;
    if (!BITTEST(m_KEYCNT.w, 15) ? irqcause == 0 : irqcause != irqbits) {return;}
    gbaControl::RequestInterrupt(gbaControl::IRQ_KEYPAD);
}

void WriteKEYCNT_B0(u8 byte) {m_KEYCNT.b.b0.b = byte;}
void WriteKEYCNT_B1(u8 byte) {m_KEYCNT.b.b1.b = byte & 0xC3;}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    UNPACK_IO_BYTES(data)
    BEGIN_IO_TABLE(base, width)
        IO_WRITE_CALLBACK(0x04000132, WriteKEYCNT_B0)
        IO_WRITE_CALLBACK(0x04000133, WriteKEYCNT_B1)
    END_IO_TABLE()
}

void ReadIO(u32 address, t32 *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    UNPACK_IO_POINTERS(data)
    BEGIN_IO_TABLE(base, width)
        IO_READ_DIRECT(0x04000130, m_KEYINPUT.b.b0.b)
        IO_READ_DIRECT(0x04000131, m_KEYINPUT.b.b1.b)
        IO_READ_DIRECT(0x04000132, m_KEYCNT.b.b0.b)
        IO_READ_DIRECT(0x04000133, m_KEYCNT.b.b1.b)
    END_IO_TABLE()
}
}
//*************************************************************************************************