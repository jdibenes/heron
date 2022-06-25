//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "../emulator.h"
#include "gba_dma.h"

namespace gbaDMA {
enum TransferTiming {
    TIMING_IMMEDIATE = 0,
    TIMING_VBLANK    = 1,
    TIMING_HBLANK    = 2,
    TIMING_SPECIAL   = 3
};

class gbaDMAChannel {
private:
    gbaControl::InterruptFlag m_IRQ;
    u32                       m_dstadrmask;
    u32                       m_srcadrmask;
    u32                       m_wordcountmask;

    t32  m_DMAXSAD;
    t32  m_DMAXDAD;
    t32  m_DMAXCNT;
    u32  m_srcadr;
    u32  m_dstadr;    
    u32  m_wordcount;
    bool m_firstaccess;
    bool m_active;

    void ReloadDAD();
    void ReloadSAD();
    void ReloadCNT_L();
    void ReloadOnStart();
    void ReloadOnRepeat();    
    void Disable();

protected:
    gbaDMAChannel(gbaControl::InterruptFlag irq, u32 dstadrmask, u32 srcadrmask, u32 wordcountmask);

public:
    void Reset();
    void OnImmediate();
    void OnVblank();
    void OnHblank();
    void OnFIFORequest(u32 fifo);
    void OnCaptureRequest();
    void OnCaptureEnd();
    bool IsEnabled() const;
    bool IsActive() const;
    s32 GetTiming() const;
    gbaMemory::DataType GetWidth() const;
    bool IsFIFOMode() const;
    bool IsCaptureMode() const;
    s32 Transfer(s32 limit);
    void WriteDMAXSAD_B0(u8 byte);
    void WriteDMAXSAD_B1(u8 byte);
    void WriteDMAXSAD_B2(u8 byte);
    void WriteDMAXSAD_B3(u8 byte);
    void WriteDMAXDAD_B0(u8 byte);
    void WriteDMAXDAD_B1(u8 byte);
    void WriteDMAXDAD_B2(u8 byte);
    void WriteDMAXDAD_B3(u8 byte);
    void WriteDMAXCNT_B0(u8 byte);
    void WriteDMAXCNT_B1(u8 byte);
    void WriteDMAXCNT_B2(u8 byte);
    void WriteDMAXCNT_B3(u8 byte);
    u8 ReadDMAXCNT_B2() const;
    u8 ReadDMAXCNT_B3() const;
};

class gbaDMA0 : public gbaDMAChannel {public: gbaDMA0();};
class gbaDMA1 : public gbaDMAChannel {public: gbaDMA1();};
class gbaDMA2 : public gbaDMAChannel {public: gbaDMA2();};
class gbaDMA3 : public gbaDMAChannel {public: gbaDMA3();};

gbaDMA0 m_DMA0;
gbaDMA1 m_DMA1;
gbaDMA2 m_DMA2;
gbaDMA3 m_DMA3;

gbaDMAChannel::gbaDMAChannel(gbaControl::InterruptFlag irq, u32 dstadrmask, u32 srcadrmask, u32 wordcountmask) {
    m_IRQ           = irq;
    m_dstadrmask    = dstadrmask;
    m_srcadrmask    = srcadrmask;
    m_wordcountmask = wordcountmask;
    Reset();
}

gbaDMA0::gbaDMA0() : gbaDMAChannel(gbaControl::IRQ_DMA0, 0x07FFFFFF, 0x07FFFFFF, 0x3FFF) {}
gbaDMA1::gbaDMA1() : gbaDMAChannel(gbaControl::IRQ_DMA1, 0x07FFFFFF, 0x0FFFFFFF, 0x3FFF) {}
gbaDMA2::gbaDMA2() : gbaDMAChannel(gbaControl::IRQ_DMA2, 0x07FFFFFF, 0x0FFFFFFF, 0x3FFF) {}
gbaDMA3::gbaDMA3() : gbaDMAChannel(gbaControl::IRQ_DMA3, 0x0FFFFFFF, 0x0FFFFFFF, 0xFFFF) {}

void gbaDMAChannel::Reset() {
    m_DMAXSAD.d   = 0;
    m_DMAXDAD.d   = 0;
    m_DMAXCNT.d   = 0;
    m_srcadr      = 0;
    m_dstadr      = 0;    
    m_wordcount   = 0;
    m_firstaccess = false;
    m_active      = false;
}

void gbaDMAChannel::ReloadDAD() {m_dstadr = m_DMAXDAD.d & m_dstadrmask;}
void gbaDMAChannel::ReloadSAD() {m_srcadr = m_DMAXSAD.d & m_srcadrmask;}

void gbaDMAChannel::ReloadCNT_L() {
    m_wordcount   = IsFIFOMode() ? 4 : ((((u32)m_DMAXCNT.w.w0.w - 1) & m_wordcountmask) + 1);
    m_firstaccess = true;
}

void gbaDMAChannel::ReloadOnStart() {
    ReloadDAD();
    ReloadSAD();
    ReloadCNT_L();
}

void gbaDMAChannel::ReloadOnRepeat() {
    ReloadCNT_L();
    if (SUBVAL(m_DMAXCNT.w.w1.w, 5, 3) == 3) {ReloadDAD();}
}

void gbaDMAChannel::Disable() {m_DMAXCNT.w.w1.w &= ~BIT(15);}

void gbaDMAChannel::OnImmediate()           {if (GetTiming() == TIMING_IMMEDIATE)     {m_active = IsEnabled();}}
void gbaDMAChannel::OnVblank()              {if (GetTiming() == TIMING_VBLANK)        {m_active = IsEnabled();}}
void gbaDMAChannel::OnHblank()              {if (GetTiming() == TIMING_HBLANK)        {m_active = IsEnabled();}}
void gbaDMAChannel::OnFIFORequest(u32 fifo) {if (IsFIFOMode() && fifo == m_DMAXDAD.d) {m_active = IsEnabled();}}
void gbaDMAChannel::OnCaptureRequest()      {if (IsCaptureMode())                     {m_active = IsEnabled();}}

void gbaDMAChannel::OnCaptureEnd() {if (IsCaptureMode()) {Disable();}}

bool gbaDMAChannel::IsEnabled() const {return BITTEST(m_DMAXCNT.w.w1.w, 15);}
bool gbaDMAChannel::IsActive() const {return m_active;}
s32 gbaDMAChannel::GetTiming() const {return SUBVAL(m_DMAXCNT.w.w1.w, 12, 3);}
gbaMemory::DataType gbaDMAChannel::GetWidth() const {return IsFIFOMode() || BITTEST(m_DMAXCNT.w.w1.w, 10) ? gbaMemory::TYPE_WORD : gbaMemory::TYPE_HALFWORD;}
bool gbaDMAChannel::IsFIFOMode() const {return (m_IRQ == gbaControl::IRQ_DMA1 || m_IRQ == gbaControl::IRQ_DMA2) && GetTiming() == TIMING_SPECIAL;}
bool gbaDMAChannel::IsCaptureMode() const {return m_IRQ == gbaControl::IRQ_DMA3 && GetTiming() == TIMING_SPECIAL;}

s32 gbaDMAChannel::Transfer(s32 limit) {
    gbaMemory::DataType width;
    s32 NR, SR, NW, SW, ret;
    t32 data;

    if (m_wordcount == 0) {ReloadOnRepeat();}

    ret = 0;

    while (ret < limit && m_wordcount > 0) {
        width = GetWidth();

        gbaMemory::Read(m_srcadr, &data, width, &NR, &SR);
        gbaMemory::Write(m_dstadr, &data, width, &NW, &SW);

        ret += m_firstaccess ? (NR + NW) : (SR + SW);
        m_firstaccess = false;

        switch (SUBVAL(m_DMAXCNT.w.w1.w, 7, 3)) {
        case 0: m_srcadr += width; break;
        case 1: m_srcadr -= width; break;
        case 2:                    break;
        }

        if (!IsFIFOMode()) {
            switch (SUBVAL(m_DMAXCNT.w.w1.w, 5, 3)) {
            case 0:
            case 3: m_dstadr += width; break;
            case 1: m_dstadr -= width; break;
            case 2:                    break;
            }
        }

        m_srcadr &= m_srcadrmask;
        m_dstadr &= m_dstadrmask;

        --m_wordcount;
    }

    if (m_wordcount == 0) {
        m_active = false;
        if (BITTEST(m_DMAXCNT.w.w1.w, 14)) {gbaControl::RequestInterrupt(m_IRQ);}
        if (!BITTEST(m_DMAXCNT.w.w1.w, 9) || GetTiming() == TIMING_IMMEDIATE) {Disable();}
    }

    return ret;
}

void gbaDMAChannel::WriteDMAXSAD_B0(u8 byte) {m_DMAXSAD.w.w0.b.b0.b = byte;}
void gbaDMAChannel::WriteDMAXSAD_B1(u8 byte) {m_DMAXSAD.w.w0.b.b1.b = byte;}
void gbaDMAChannel::WriteDMAXSAD_B2(u8 byte) {m_DMAXSAD.w.w1.b.b0.b = byte;}
void gbaDMAChannel::WriteDMAXSAD_B3(u8 byte) {m_DMAXSAD.w.w1.b.b1.b = byte;}

void gbaDMAChannel::WriteDMAXDAD_B0(u8 byte) {m_DMAXDAD.w.w0.b.b0.b = byte;}
void gbaDMAChannel::WriteDMAXDAD_B1(u8 byte) {m_DMAXDAD.w.w0.b.b1.b = byte;}
void gbaDMAChannel::WriteDMAXDAD_B2(u8 byte) {m_DMAXDAD.w.w1.b.b0.b = byte;}
void gbaDMAChannel::WriteDMAXDAD_B3(u8 byte) {m_DMAXDAD.w.w1.b.b1.b = byte;}

void gbaDMAChannel::WriteDMAXCNT_B0(u8 byte) {m_DMAXCNT.w.w0.b.b0.b = byte;}
void gbaDMAChannel::WriteDMAXCNT_B1(u8 byte) {m_DMAXCNT.w.w0.b.b1.b = byte;}
void gbaDMAChannel::WriteDMAXCNT_B2(u8 byte) {m_DMAXCNT.w.w1.b.b0.b = byte & 0xE0;}

void gbaDMAChannel::WriteDMAXCNT_B3(u8 byte) {
    bool enable = !IsEnabled() && BITTEST(byte, 7);
    m_DMAXCNT.w.w1.b.b1.b = byte & (m_IRQ != gbaControl::IRQ_DMA3 ? 0xF7 : 0xFF);
    if (enable) {ReloadOnStart(); OnImmediate();}   
}

u8 gbaDMAChannel::ReadDMAXCNT_B2() const {return m_DMAXCNT.w.w1.b.b0.b;}
u8 gbaDMAChannel::ReadDMAXCNT_B3() const {return m_DMAXCNT.w.w1.b.b1.b;}

void Reset() {
    m_DMA0.Reset();
    m_DMA1.Reset();
    m_DMA2.Reset();
    m_DMA3.Reset();
}

s32 Sync(s32 limit) {
         if (m_DMA0.IsActive()) {return m_DMA0.Transfer(limit);}
    else if (m_DMA1.IsActive()) {return m_DMA1.Transfer(limit);}
    else if (m_DMA2.IsActive()) {return m_DMA2.Transfer(limit);}
    else if (m_DMA3.IsActive()) {return m_DMA3.Transfer(limit);}
    else                        {return 0;}
}

bool IsSyncPending() {return m_DMA0.IsActive() || m_DMA1.IsActive() || m_DMA2.IsActive() || m_DMA3.IsActive();}

void OnVblank() {
    m_DMA0.OnVblank();
    m_DMA1.OnVblank();
    m_DMA2.OnVblank();
    m_DMA3.OnVblank();
}

void OnHblank() {
    m_DMA0.OnHblank();
    m_DMA1.OnHblank();
    m_DMA2.OnHblank();
    m_DMA3.OnHblank();
}

void OnFIFORequest(u32 fifo) {
    m_DMA1.OnFIFORequest(fifo);
    m_DMA2.OnFIFORequest(fifo);
}

void OnCaptureRequest() {
    m_DMA3.OnCaptureRequest();
}

void OnCaptureEnd() {
    m_DMA3.OnCaptureEnd();
}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    UNPACK_IO_BYTES(data);
    BEGIN_IO_TABLE(base, width)
        IO_WRITE_CALLBACK(0x040000B0, m_DMA0.WriteDMAXSAD_B0)
        IO_WRITE_CALLBACK(0x040000B1, m_DMA0.WriteDMAXSAD_B1)
        IO_WRITE_CALLBACK(0x040000B2, m_DMA0.WriteDMAXSAD_B2)
        IO_WRITE_CALLBACK(0x040000B3, m_DMA0.WriteDMAXSAD_B3)
        IO_WRITE_CALLBACK(0x040000B4, m_DMA0.WriteDMAXDAD_B0)
        IO_WRITE_CALLBACK(0x040000B5, m_DMA0.WriteDMAXDAD_B1)
        IO_WRITE_CALLBACK(0x040000B6, m_DMA0.WriteDMAXDAD_B2)
        IO_WRITE_CALLBACK(0x040000B7, m_DMA0.WriteDMAXDAD_B3)
        IO_WRITE_CALLBACK(0x040000B8, m_DMA0.WriteDMAXCNT_B0)
        IO_WRITE_CALLBACK(0x040000B9, m_DMA0.WriteDMAXCNT_B1)
        IO_WRITE_CALLBACK(0x040000BA, m_DMA0.WriteDMAXCNT_B2)
        IO_WRITE_CALLBACK(0x040000BB, m_DMA0.WriteDMAXCNT_B3)
        IO_WRITE_CALLBACK(0x040000BC, m_DMA1.WriteDMAXSAD_B0)
        IO_WRITE_CALLBACK(0x040000BD, m_DMA1.WriteDMAXSAD_B1)
        IO_WRITE_CALLBACK(0x040000BE, m_DMA1.WriteDMAXSAD_B2)
        IO_WRITE_CALLBACK(0x040000BF, m_DMA1.WriteDMAXSAD_B3)
        IO_WRITE_CALLBACK(0x040000C0, m_DMA1.WriteDMAXDAD_B0)
        IO_WRITE_CALLBACK(0x040000C1, m_DMA1.WriteDMAXDAD_B1)
        IO_WRITE_CALLBACK(0x040000C2, m_DMA1.WriteDMAXDAD_B2)
        IO_WRITE_CALLBACK(0x040000C3, m_DMA1.WriteDMAXDAD_B3)
        IO_WRITE_CALLBACK(0x040000C4, m_DMA1.WriteDMAXCNT_B0)
        IO_WRITE_CALLBACK(0x040000C5, m_DMA1.WriteDMAXCNT_B1)
        IO_WRITE_CALLBACK(0x040000C6, m_DMA1.WriteDMAXCNT_B2)
        IO_WRITE_CALLBACK(0x040000C7, m_DMA1.WriteDMAXCNT_B3)
        IO_WRITE_CALLBACK(0x040000C8, m_DMA2.WriteDMAXSAD_B0)
        IO_WRITE_CALLBACK(0x040000C9, m_DMA2.WriteDMAXSAD_B1)
        IO_WRITE_CALLBACK(0x040000CA, m_DMA2.WriteDMAXSAD_B2)
        IO_WRITE_CALLBACK(0x040000CB, m_DMA2.WriteDMAXSAD_B3)
        IO_WRITE_CALLBACK(0x040000CC, m_DMA2.WriteDMAXDAD_B0)
        IO_WRITE_CALLBACK(0x040000CD, m_DMA2.WriteDMAXDAD_B1)
        IO_WRITE_CALLBACK(0x040000CE, m_DMA2.WriteDMAXDAD_B2)
        IO_WRITE_CALLBACK(0x040000CF, m_DMA2.WriteDMAXDAD_B3)
        IO_WRITE_CALLBACK(0x040000D0, m_DMA2.WriteDMAXCNT_B0)
        IO_WRITE_CALLBACK(0x040000D1, m_DMA2.WriteDMAXCNT_B1)
        IO_WRITE_CALLBACK(0x040000D2, m_DMA2.WriteDMAXCNT_B2)
        IO_WRITE_CALLBACK(0x040000D3, m_DMA2.WriteDMAXCNT_B3)        
        IO_WRITE_CALLBACK(0x040000D4, m_DMA3.WriteDMAXSAD_B0)
        IO_WRITE_CALLBACK(0x040000D5, m_DMA3.WriteDMAXSAD_B1)
        IO_WRITE_CALLBACK(0x040000D6, m_DMA3.WriteDMAXSAD_B2)
        IO_WRITE_CALLBACK(0x040000D7, m_DMA3.WriteDMAXSAD_B3)
        IO_WRITE_CALLBACK(0x040000D8, m_DMA3.WriteDMAXDAD_B0)
        IO_WRITE_CALLBACK(0x040000D9, m_DMA3.WriteDMAXDAD_B1)
        IO_WRITE_CALLBACK(0x040000DA, m_DMA3.WriteDMAXDAD_B2)
        IO_WRITE_CALLBACK(0x040000DB, m_DMA3.WriteDMAXDAD_B3)
        IO_WRITE_CALLBACK(0x040000DC, m_DMA3.WriteDMAXCNT_B0)
        IO_WRITE_CALLBACK(0x040000DD, m_DMA3.WriteDMAXCNT_B1)
        IO_WRITE_CALLBACK(0x040000DE, m_DMA3.WriteDMAXCNT_B2)
        IO_WRITE_CALLBACK(0x040000DF, m_DMA3.WriteDMAXCNT_B3)
    END_IO_TABLE()
}

void ReadIO(u32 address, t32 *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    UNPACK_IO_POINTERS(data);
    BEGIN_IO_TABLE(base, width)
        IO_READ_DIRECT(0x040000B8, 0)
        IO_READ_DIRECT(0x040000B9, 0)
        IO_READ_CALLBACK(0x040000BA, m_DMA0.ReadDMAXCNT_B2)
        IO_READ_CALLBACK(0x040000BB, m_DMA0.ReadDMAXCNT_B3)
        IO_READ_DIRECT(0x040000C4, 0)
        IO_READ_DIRECT(0x040000C5, 0)
        IO_READ_CALLBACK(0x040000C6, m_DMA1.ReadDMAXCNT_B2)
        IO_READ_CALLBACK(0x040000C7, m_DMA1.ReadDMAXCNT_B3)
        IO_READ_DIRECT(0x040000D0, 0)
        IO_READ_DIRECT(0x040000D1, 0)
        IO_READ_CALLBACK(0x040000D2, m_DMA2.ReadDMAXCNT_B2)
        IO_READ_CALLBACK(0x040000D3, m_DMA2.ReadDMAXCNT_B3)
        IO_READ_DIRECT(0x040000DC, 0)
        IO_READ_DIRECT(0x040000DD, 0)
        IO_READ_CALLBACK(0x040000DE, m_DMA3.ReadDMAXCNT_B2)
        IO_READ_CALLBACK(0x040000DF, m_DMA3.ReadDMAXCNT_B3)
    END_IO_TABLE()
}
}
//*************************************************************************************************