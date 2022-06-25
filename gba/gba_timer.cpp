//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "../emulator.h"
#include "gba_control.h"
#include "gba_sound.h"
#include "gba_timer.h"

namespace gbaTimer {
class gbaTimerBase {
private:
    gbaControl::InterruptFlag m_IRQ;
    gbaTimerBase             *m_cup;

    t16 m_counter;
    t16 m_reload;
    t16 m_TMXCNT_H;

    void ReloadCounter();
    void OnOverflow();
    void Increment();

protected:
    gbaTimerBase(gbaControl::InterruptFlag irq, gbaTimerBase *cup);

public:
    void Reset();
    void PrescalerSync(s32 clk);
    bool IsEnabled() const;
    bool IsSlave() const;
    u16 GetCounter() const;
    u32 GetPrescaler() const;
    void WriteTMXCNT_L_B0(u8 byte);
    void WriteTMXCNT_L_B1(u8 byte);
    void WriteTMXCNT_H_B0(u8 byte);
    u8 ReadTMXCNT_L_B0() const;
    u8 ReadTMXCNT_L_B1() const;
    u8 ReadTMXCNT_H_B0() const;
};

class gbaTimer3 : public gbaTimerBase {public: gbaTimer3();};
class gbaTimer2 : public gbaTimerBase {public: gbaTimer2();};
class gbaTimer1 : public gbaTimerBase {public: gbaTimer1();};
class gbaTimer0 : public gbaTimerBase {public: gbaTimer0();};

const s32 m_prescalerclk[4] = {1, 64, 256, 1024};

s32 m_prescalerticks[4];
s32 m_prescalercount[4];

gbaTimer3 m_timer3;
gbaTimer2 m_timer2;
gbaTimer1 m_timer1;
gbaTimer0 m_timer0;

gbaTimerBase::gbaTimerBase(gbaControl::InterruptFlag irq, gbaTimerBase *cup) {
    m_IRQ = irq;
    m_cup = cup;
    Reset();
}

void gbaTimerBase::ReloadCounter() {m_counter.w = m_reload.w;}

void gbaTimerBase::OnOverflow() {
    ReloadCounter();
    if (BITTEST(m_TMXCNT_H.w, 6)) {gbaControl::RequestInterrupt(m_IRQ);}
    if (m_IRQ == gbaControl::IRQ_TIMER0 || m_IRQ == gbaControl::IRQ_TIMER1) {gbaSound::OnTimerOverflow(m_IRQ);}
    if (m_cup != 0 && m_cup->IsEnabled() && m_cup->IsSlave()) {m_cup->Increment();}
}

void gbaTimerBase::Increment() {if (++m_counter.w == 0) {OnOverflow();}}

void gbaTimerBase::Reset() {
    m_counter.w  = 0;
    m_reload.w   = 0;
    m_TMXCNT_H.w = 0;
}

void gbaTimerBase::PrescalerSync(s32 clk) {
    u32 diff;
    if (IsEnabled() && !IsSlave()) {
        while (clk > 0) {
            diff = 0x10000 - m_counter.w;
            if ((u32)clk >= diff) {
                OnOverflow();
                clk -= diff;
            }
            else {
                m_counter.w += clk & 0xFFFF;
                clk = 0;
            }
        }
    }
}

bool gbaTimerBase::IsEnabled() const {return BITTEST(m_TMXCNT_H.w, 7);}
bool gbaTimerBase::IsSlave() const {return m_IRQ != gbaControl::IRQ_TIMER0 && BITTEST(m_TMXCNT_H.w, 2);}
u16 gbaTimerBase::GetCounter() const {return m_counter.w;}
u32 gbaTimerBase::GetPrescaler() const {return SUBVAL(m_TMXCNT_H.w, 0, 0x03);}

void gbaTimerBase::WriteTMXCNT_L_B0(u8 byte) {m_reload.b.b0.b = byte;}
void gbaTimerBase::WriteTMXCNT_L_B1(u8 byte) {m_reload.b.b1.b = byte;}

void gbaTimerBase::WriteTMXCNT_H_B0(u8 byte) {
    if (!IsEnabled() && BITTEST(byte, 7)) {ReloadCounter();}
    m_TMXCNT_H.w = byte & (m_IRQ != gbaControl::IRQ_TIMER0 ? 0xC7 : 0xC3);
}

u8 gbaTimerBase::ReadTMXCNT_L_B0() const {return m_counter.b.b0.b;}
u8 gbaTimerBase::ReadTMXCNT_L_B1() const {return m_counter.b.b1.b;}
u8 gbaTimerBase::ReadTMXCNT_H_B0() const {return m_TMXCNT_H.b.b0.b;}

gbaTimer3::gbaTimer3() : gbaTimerBase(gbaControl::IRQ_TIMER3, 0        ) {}
gbaTimer2::gbaTimer2() : gbaTimerBase(gbaControl::IRQ_TIMER2, &m_timer3) {}
gbaTimer1::gbaTimer1() : gbaTimerBase(gbaControl::IRQ_TIMER1, &m_timer2) {}
gbaTimer0::gbaTimer0() : gbaTimerBase(gbaControl::IRQ_TIMER0, &m_timer1) {}

void ResetPrescaler() {for (s32 i = 0; i < 4; i++) {m_prescalerticks[i] = m_prescalerclk[i]; m_prescalercount[i] = 0;}}

void Reset() {
    m_timer0.Reset();
    m_timer1.Reset();
    m_timer2.Reset();
    m_timer3.Reset();
    ResetPrescaler();
}

void SyncPrescaler(s32 ticks) {
    for (s32 i = 0; i < 4; i++) {
        m_prescalerticks[i] -= ticks & (m_prescalerclk[i] - 1);
        if (m_prescalerticks[i] > 0) {m_prescalercount[i] = 0;} else {m_prescalercount[i] = 1; m_prescalerticks[i] += m_prescalerclk[i];}
        m_prescalercount[i] += ticks / m_prescalerclk[i];
    }
}

void Sync(s32 ticks) {
    if (ticks <= 0) {return;}
    SyncPrescaler(ticks);
    m_timer0.PrescalerSync(m_prescalercount[m_timer0.GetPrescaler()]);
    m_timer1.PrescalerSync(m_prescalercount[m_timer1.GetPrescaler()]);
    m_timer2.PrescalerSync(m_prescalercount[m_timer2.GetPrescaler()]);
    m_timer3.PrescalerSync(m_prescalercount[m_timer3.GetPrescaler()]);
}

s32 GetNextEvent() {
    s32 ret = 0;
    u32 psc;
    s32 tov;
    if (m_timer0.IsEnabled())                        {psc = m_timer0.GetPrescaler(); tov = m_prescalerticks[psc] + ((0xFFFF - m_timer0.GetCounter()) * m_prescalerclk[psc]);                             ret = tov; }
    if (m_timer1.IsEnabled() && !m_timer1.IsSlave()) {psc = m_timer1.GetPrescaler(); tov = m_prescalerticks[psc] + ((0xFFFF - m_timer1.GetCounter()) * m_prescalerclk[psc]); if (ret <= 0 || tov < ret) {ret = tov;}}
    if (m_timer2.IsEnabled() && !m_timer2.IsSlave()) {psc = m_timer2.GetPrescaler(); tov = m_prescalerticks[psc] + ((0xFFFF - m_timer2.GetCounter()) * m_prescalerclk[psc]); if (ret <= 0 || tov < ret) {ret = tov;}}
    if (m_timer3.IsEnabled() && !m_timer3.IsSlave()) {psc = m_timer3.GetPrescaler(); tov = m_prescalerticks[psc] + ((0xFFFF - m_timer3.GetCounter()) * m_prescalerclk[psc]); if (ret <= 0 || tov < ret) {ret = tov;}}
    return ret;
}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    UNPACK_IO_BYTES(data)
    BEGIN_IO_TABLE(base, width)
        IO_WRITE_CALLBACK(0x04000100, m_timer0.WriteTMXCNT_L_B0)
        IO_WRITE_CALLBACK(0x04000101, m_timer0.WriteTMXCNT_L_B1)
        IO_WRITE_CALLBACK(0x04000102, m_timer0.WriteTMXCNT_H_B0)
        IO_WRITE_CALLBACK(0x04000104, m_timer1.WriteTMXCNT_L_B0)
        IO_WRITE_CALLBACK(0x04000105, m_timer1.WriteTMXCNT_L_B1)
        IO_WRITE_CALLBACK(0x04000106, m_timer1.WriteTMXCNT_H_B0)
        IO_WRITE_CALLBACK(0x04000108, m_timer2.WriteTMXCNT_L_B0)
        IO_WRITE_CALLBACK(0x04000109, m_timer2.WriteTMXCNT_L_B1)
        IO_WRITE_CALLBACK(0x0400010A, m_timer2.WriteTMXCNT_H_B0)        
        IO_WRITE_CALLBACK(0x0400010C, m_timer3.WriteTMXCNT_L_B0)
        IO_WRITE_CALLBACK(0x0400010D, m_timer3.WriteTMXCNT_L_B1)
        IO_WRITE_CALLBACK(0x0400010E, m_timer3.WriteTMXCNT_H_B0)
    END_IO_TABLE()
}

void ReadIO(u32 address, t32 *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    UNPACK_IO_POINTERS(data)
    BEGIN_IO_TABLE(base, width)
        IO_READ_CALLBACK(0x04000100, m_timer0.ReadTMXCNT_L_B0)
        IO_READ_CALLBACK(0x04000101, m_timer0.ReadTMXCNT_L_B1)
        IO_READ_CALLBACK(0x04000102, m_timer0.ReadTMXCNT_H_B0)
        IO_READ_DIRECT(0x04000103, 0)
        IO_READ_CALLBACK(0x04000104, m_timer1.ReadTMXCNT_L_B0)
        IO_READ_CALLBACK(0x04000105, m_timer1.ReadTMXCNT_L_B1)
        IO_READ_CALLBACK(0x04000106, m_timer1.ReadTMXCNT_H_B0)
        IO_READ_DIRECT(0x04000107, 0)
        IO_READ_CALLBACK(0x04000108, m_timer2.ReadTMXCNT_L_B0)
        IO_READ_CALLBACK(0x04000109, m_timer2.ReadTMXCNT_L_B1)
        IO_READ_CALLBACK(0x0400010A, m_timer2.ReadTMXCNT_H_B0)
        IO_READ_DIRECT(0x0400010B, 0)
        IO_READ_CALLBACK(0x0400010C, m_timer3.ReadTMXCNT_L_B0)
        IO_READ_CALLBACK(0x0400010D, m_timer3.ReadTMXCNT_L_B1)
        IO_READ_CALLBACK(0x0400010E, m_timer3.ReadTMXCNT_H_B0)
        IO_READ_DIRECT(0x0400010F, 0)
    END_IO_TABLE()
}
}
//*************************************************************************************************