//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "gba_sio.h"

namespace gbaSIO
{
enum ModeSelection
{
    MODE_NORMAL,
    MODE_MULTIPLAYER,
    MODE_UART,
    MODE_GENERALPURPOSE,
    MODE_JOYBUS
};

const u8 m_SC = BIT(0);
const u8 m_SD = BIT(1);
const u8 m_SI = BIT(2);
const u8 m_SO = BIT(3);

t16 m_SIOMULTI[4];
t16 m_SIOCNT; // 0x0004
t16 m_SIODATA8;
t16 m_RCNT; // 0x800F
t8  m_JOYCNT;
t32 m_JOYRECV;
t32 m_JOYTRANS;
t8  m_JOYSTAT;

ModeSelection m_mode;

void UpdateSIOState()
{
    if (BITTEST(m_RCNT.w, 15))
    {
        m_mode = !BITTEST(m_RCNT.w, 14) ? MODE_GENERALPURPOSE : MODE_JOYBUS;
    }
    else
    {
        switch (SUBVAL(m_SIOCNT.w, 12, 3))
        {
        case 0:
        case 1:  m_mode = MODE_NORMAL;      break;
        case 2:  m_mode = MODE_MULTIPLAYER; break;
        default: m_mode = MODE_UART;        break;
        }
    }

    u16 rcnt   = m_RCNT.w;
    u16 siocnt = m_SIOCNT.w;

    switch (m_mode)
    {
    case MODE_NORMAL:
        rcnt |= m_SC | m_SI;
        if (BITTEST(siocnt, 0)) {rcnt |= m_SD;} else {rcnt &= ~m_SD;}
        if (!BITTEST(siocnt, 7) && !BITTEST(siocnt, 3)) {rcnt &= ~m_SO;} else {rcnt |= m_SO;}
        siocnt |= BIT(2);
        break;
    case MODE_MULTIPLAYER:
        rcnt    = (rcnt | m_SC | m_SO) & ~(m_SD | m_SI);
        siocnt &= ~(BIT(2) | BIT(3));
        if (BITTEST(siocnt, 7)) {for (u32 i = 0; i < 4; i++) {m_SIOMULTI[i].w = 0xFFFF;}}
        break;
    case MODE_UART:
        rcnt |= m_SC | m_SD | m_SI | m_SO;
        break;
    case MODE_GENERALPURPOSE:
        if (!BITTEST(rcnt, 4)) {rcnt |= m_SC;}
        if (!BITTEST(rcnt, 5)) {rcnt |= m_SD;}
        if (!BITTEST(rcnt, 6)) {rcnt |= m_SI;}
        if (!BITTEST(rcnt, 7)) {rcnt |= m_SO;}
        break;
    case MODE_JOYBUS:
        rcnt = (rcnt | m_SI | m_SO) & ~(m_SC | m_SD);
        break;
    }

    m_RCNT.w   = rcnt;
    m_SIOCNT.w = siocnt;
}

void Reset()
{
    for (u32 i = 0; i < 4; i++) {m_SIOMULTI[i].w = 0;}

    m_SIOCNT.w   = 0;
    m_SIODATA8.w = 0;
    m_RCNT.w     = 0;
    m_JOYCNT.b   = 0;
    m_JOYRECV.d  = 0;
    m_JOYTRANS.d = 0;
    m_JOYSTAT.b  = 0;

    UpdateSIOState();
}

void WriteSIOCNT_B0(u8 byte)
{
    m_SIOCNT.b.b0.b = byte & ~(BIT(4) | BIT(5) | BIT(6));
    UpdateSIOState();
}

void WriteSIOCNT_B1(u8 byte)
{
    m_SIOCNT.b.b1.b = byte & ~BIT(7);
    UpdateSIOState();
}

void WriteRCNT_B0(u8 byte)
{
    m_RCNT.b.b0.b = byte;
    UpdateSIOState();
}

void WriteRCNT_B1(u8 byte)
{
    m_RCNT.b.b1.b = byte & ~(BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5));
    UpdateSIOState();
}

void WriteJOYCNT(u8 byte)
{
    m_JOYCNT.b = (m_JOYCNT.b & 0x7 & ~byte) | (byte & 0xF8);
}

void WriteJOYTRANS_B0(u8 byte)
{
    m_JOYSTAT.b |= BIT(1);
    m_JOYTRANS.w.w0.b.b0.b = byte;
}

void WriteJOYTRANS_B1(u8 byte)
{
    m_JOYSTAT.b |= BIT(1);
    m_JOYTRANS.w.w0.b.b1.b = byte;
}

void WriteJOYTRANS_B2(u8 byte)
{
    m_JOYSTAT.b |= BIT(1);
    m_JOYTRANS.w.w1.b.b0.b = byte;
}

void WriteJOYTRANS_B3(u8 byte)
{
    m_JOYSTAT.b |= BIT(1);
    m_JOYTRANS.w.w1.b.b1.b = byte;
}

u8 ReadSIOCNT_B0()
{
    if (m_mode == MODE_UART) {m_SIOCNT.w &= ~BIT(6);}
    return m_SIOCNT.b.b0.b;
}

u8 ReadSIOCNT_B1()
{
    return m_SIOCNT.b.b1.b;
}

u8 ReadJOYRECV_B0()
{
    m_JOYSTAT.b &= ~BIT(3);
    return m_JOYRECV.w.w0.b.b0.b;
}

u8 ReadJOYRECV_B1()
{
    m_JOYSTAT.b &= ~BIT(3);
    return m_JOYRECV.w.w0.b.b1.b;
}

u8 ReadJOYRECV_B2()
{
    m_JOYSTAT.b &= ~BIT(3);
    return m_JOYRECV.w.w1.b.b0.b;
}

u8 ReadJOYRECV_B3()
{
    m_JOYSTAT.b &= ~BIT(3);
    return m_JOYRECV.w.w1.b.b1.b;
}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width)
{
    u8  bytes[4] = {data->w.w0.b.b0.b, data->w.w0.b.b1.b, data->w.w1.b.b0.b, data->w.w1.b.b1.b};
    u32 base     = address & ~(width - 1);

    for (u32 i = 0; i < (u32)width; i++)
    {
        switch (base + i)
        {
        case 0x04000120: m_SIOMULTI[0].b.b0.b = bytes[i];  break;
        case 0x04000121: m_SIOMULTI[0].b.b1.b = bytes[i];  break;
        case 0x04000122: m_SIOMULTI[1].b.b0.b = bytes[i];  break;
        case 0x04000123: m_SIOMULTI[1].b.b1.b = bytes[i];  break;
        case 0x04000124: m_SIOMULTI[2].b.b0.b = bytes[i];  break;
        case 0x04000125: m_SIOMULTI[2].b.b1.b = bytes[i];  break;
        case 0x04000126: m_SIOMULTI[3].b.b0.b = bytes[i];  break;
        case 0x04000127: m_SIOMULTI[3].b.b1.b = bytes[i];  break;
        case 0x04000128: WriteSIOCNT_B0(bytes[i]);         break;
        case 0x04000129: WriteSIOCNT_B1(bytes[i]);         break;
        case 0x0400012A: m_SIODATA8.b.b0.b = bytes[i];     break;
        case 0x0400012B: m_SIODATA8.b.b1.b = bytes[i];     break;
        case 0x04000134: WriteRCNT_B0(bytes[i]);           break;
        case 0x04000135: WriteRCNT_B1(bytes[i]);           break;
        case 0x04000140: WriteJOYCNT(bytes[i]);            break;
        case 0x04000150: m_JOYRECV.w.w0.b.b0.b = bytes[i]; break;
        case 0x04000151: m_JOYRECV.w.w0.b.b1.b = bytes[i]; break;
        case 0x04000152: m_JOYRECV.w.w1.b.b0.b = bytes[i]; break;
        case 0x04000153: m_JOYRECV.w.w1.b.b1.b = bytes[i]; break;
        case 0x04000154: WriteJOYTRANS_B0(bytes[i]);       break;
        case 0x04000155: WriteJOYTRANS_B1(bytes[i]);       break;
        case 0x04000156: WriteJOYTRANS_B2(bytes[i]);       break;
        case 0x04000157: WriteJOYTRANS_B3(bytes[i]);       break;
        case 0x04000158: m_JOYSTAT.b = bytes[i];           break;
        }
    }
}

void ReadIO(u32 address, t32 *data, gbaMemory::DataType width)
{
    u8 *bytes[4] = {&data->w.w0.b.b0.b, &data->w.w0.b.b1.b, &data->w.w1.b.b0.b, &data->w.w1.b.b1.b};
    u32 base     = address & ~(width - 1);

    for (u32 i = 0; i < (u32)width; i++)
    {
        switch (base + i)
        {
        case 0x04000120: *bytes[i] = m_SIOMULTI[0].b.b0.b;   break;
        case 0x04000121: *bytes[i] = m_SIOMULTI[0].b.b1.b;   break;
        case 0x04000122: *bytes[i] = m_SIOMULTI[1].b.b0.b;   break;
        case 0x04000123: *bytes[i] = m_SIOMULTI[1].b.b1.b;   break;
        case 0x04000124: *bytes[i] = m_SIOMULTI[2].b.b0.b;   break;
        case 0x04000125: *bytes[i] = m_SIOMULTI[2].b.b1.b;   break;
        case 0x04000126: *bytes[i] = m_SIOMULTI[3].b.b0.b;   break;
        case 0x04000127: *bytes[i] = m_SIOMULTI[3].b.b1.b;   break;
        case 0x04000128: *bytes[i] = ReadSIOCNT_B0();        break;
        case 0x04000129: *bytes[i] = ReadSIOCNT_B1();        break;
        case 0x0400012A: *bytes[i] = m_SIODATA8.b.b0.b;      break;
        case 0x0400012B: *bytes[i] = m_SIODATA8.b.b1.b;      break;
        case 0x04000134: *bytes[i] = m_RCNT.b.b0.b;          break;
        case 0x04000135: *bytes[i] = m_RCNT.b.b1.b;          break;
        case 0x04000136: *bytes[i] = 0;                      break;
        case 0x04000137: *bytes[i] = 0;                      break;
        case 0x04000140: *bytes[i] = m_JOYCNT.b;             break;
        case 0x04000141: *bytes[i] = 0;                      break;
        case 0x04000142: *bytes[i] = 0;                      break;
        case 0x04000143: *bytes[i] = 0;                      break;
        case 0x04000150: *bytes[i] = ReadJOYRECV_B0();       break;
        case 0x04000151: *bytes[i] = ReadJOYRECV_B1();       break;
        case 0x04000152: *bytes[i] = ReadJOYRECV_B2();       break;
        case 0x04000153: *bytes[i] = ReadJOYRECV_B3();       break;
        case 0x04000154: *bytes[i] = m_JOYTRANS.w.w0.b.b0.b; break;
        case 0x04000155: *bytes[i] = m_JOYTRANS.w.w0.b.b1.b; break;
        case 0x04000156: *bytes[i] = m_JOYTRANS.w.w1.b.b0.b; break;
        case 0x04000157: *bytes[i] = m_JOYTRANS.w.w1.b.b1.b; break;
        case 0x04000158: *bytes[i] = m_JOYSTAT.b;            break;
        case 0x04000159: *bytes[i] = 0;                      break;
        case 0x0400015A: *bytes[i] = 0;                      break;
        case 0x0400015B: *bytes[i] = 0;                      break;
        }
    }
}
}

//*************************************************************************************************