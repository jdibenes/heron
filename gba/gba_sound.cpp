//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include <queue>
#include "../emulator.h"
#include "gba_dma.h"
#include "gba_sound.h"

namespace gbaSound
{
class gbaSquarePattern
{
private:
    static const s8 m_pattern[4][8];
    s32 m_out;
    s32 m_ticks;
    s32 m_freq;
    s32 m_dutycycle;

    void DoReloadCounter();
    void DoSync();

public:
    gbaSquarePattern();
    void Sync(s32 ticks);
    void Reset();
    void Init();
    s8 GetSample() const;
    void SetDutyCycle(s32 dutycycle);
    void SetFrequency(s32 frequency);
    s32 GetFrequency();
};

class gbaSweepUnit
{
private:
    gbaSquarePattern *m_square;
    s32 m_ticks;
    s32 m_time;
    s32 m_direction;
    s32 m_shifts;
    s32 m_counter;
    s32 m_freq;
    s32 m_nextfreq;
    bool m_off;
    bool m_active;    

    void DoReloadCounter();
    void DoSweepFrequency();
    bool DoSync();
    s32 PartialSync();

public:
    gbaSweepUnit(gbaSquarePattern *square);
    void Sync(s32 ticks);
    void Reset();
    void Init(s32 frequency);
    s32 GetFrequency() const;
    bool IsChannelOff() const;
    void SetSweepRegister(u8 NRX0);
};

class gbaSoundLength
{
private:
    s32 m_counter;
    s32 m_maxlength;
    bool m_off;
    bool m_active;
    s32 m_ticks;

    void DoReloadCounter();
    void DoSync();

public:
    gbaSoundLength();
    void Sync(s32 ticks);
    void Reset();
    void Init();
    bool IsChannelOff() const;
    void SetMaxLength(s32 maxlength);
    void SetCounter(s32 counter);
    void SetEnable(s32 enable);
};

class gbaVolumeEnvelope
{
private:
    s32 m_ticks;
    s32 m_initvol;
    s32 m_vol;
    s32 m_direction;
    s32 m_step;
    s32 m_counter;
    bool m_active;

    void DoReloadCounter();
    void DoStep();
    void DoSync();

public:
    gbaVolumeEnvelope();
    void Sync(s32 ticks);
    void Reset();
    void Init();
    s32 GetVolume() const;
    void SetVolumeEnvelopeRegister(u8 NRX2);
};

class gbaWavePattern
{
private:
    s8 m_pattern[2][16];
    s32 m_position;
    s32 m_ticks;
    s32 m_freq;
    s8 m_sample;
    s32 m_playbank;
    s32 m_databank;
    s32 m_initbank;
    bool m_use2banks;
    bool m_off;
    bool m_enable;

    void DoReloadCounter();
    void DoSync();

public:
    gbaWavePattern();
    void Sync(s32 ticks);
    void Reset(bool preservewaveram);
    void Init();
    s8 GetSample() const;
    bool IsChannelOff() const;
    void SetOutputEnable(s32 enable);
    void SetFrequency(s32 frequency);
    void SetBank(s32 bank);
    void SetDimension(bool use2banks);
    void WriteWaveRAM(u8 offset, s8 samples);
    s8 ReadWaveRAM(u8 offset);
};

class gbaNoisePattern
{
private:
    static const u32 m_pattern[2][2];
    u32 m_shiftreg;
    s32 m_ratio;
    s32 m_shiftclk;
    s32 m_width;
    s32 m_ticks;
    s8 m_sample;

    void DoReloadCounter();
    void DoSync();

public:
    gbaNoisePattern();
    void Sync(s32 ticks);
    void Reset();
    void Init();
    s8 GetSample() const;
    void SetNoiseRegister(u8 NR43);
};

class gbaSoundChannel1
{
private:
    gbaSquarePattern m_square;
    gbaSweepUnit m_sweep;
    gbaSoundLength m_length;
    gbaVolumeEnvelope m_volume;
    s32 m_freq;
    bool m_off;

public:
    gbaSoundChannel1();
    void Sync(s32 ticks);
    void Reset();
    s32 GetSample() const;
    bool IsChannelOff() const;
    void WriteNR10(u8 NR10);
    void WriteNR11(u8 NR11);
    void WriteNR12(u8 NR12);
    void WriteNR13(u8 NR13);
    void WriteNR14(u8 NR14);
};

class gbaSoundChannel2
{
private:
    gbaSquarePattern m_square;
    gbaSoundLength m_length;
    gbaVolumeEnvelope m_volume;
    s32 m_freq;
    bool m_off;

public:
    gbaSoundChannel2();
    void Sync(s32 ticks);
    void Reset();
    s32 GetSample() const;
    bool IsChannelOff() const;
    void WriteNR21(u8 NR21);
    void WriteNR22(u8 NR22);
    void WriteNR23(u8 NR23);
    void WriteNR24(u8 NR24);
};

class gbaSoundChannel3
{
private:
    gbaWavePattern m_wave;
    gbaSoundLength m_length;
    s32 m_freq;
    s32 m_level;
    bool m_off;
    bool m_forcevol;

public:
    gbaSoundChannel3();
    void Sync(s32 ticks);
    void Reset(bool preservewaveram);
    s32 GetSample() const;
    bool IsChannelOff() const;
    void WriteNR30(u8 NR30);
    void WriteNR31(u8 NR31);
    void WriteNR32(u8 NR32);
    void WriteNR33(u8 NR33);
    void WriteNR34(u8 NR34);
    void WriteWavePattern(u8 offset, u8 byte);
    u8 ReadWavePattern(u8 offset);
};

class gbaSoundChannel4
{
private:
    gbaSoundLength m_length;
    gbaVolumeEnvelope m_volume;
    gbaNoisePattern m_noise;
    bool m_off;

public:
    gbaSoundChannel4();
    void Sync(s32 ticks);
    void Reset();
    s32 GetSample() const;
    bool IsChannelOff() const;
    void WriteNR41(u8 NR41);
    void WriteNR42(u8 NR42);
    void WriteNR43(u8 NR43);
    void WriteNR44(u8 NR44);
};

class gbaDirectSound
{
private:
    std::queue<s8> m_FIFO;
    s8 m_sample;
    gbaControl::InterruptFlag m_sampleratetimer;
    u32 m_DMADRQ;

public:
    gbaDirectSound(u32 fifo);
    void Reset();
    void SetTimer(gbaControl::InterruptFlag timerid);
    gbaControl::InterruptFlag GetTimer() const;
    void ResetFIFO();
    void SendNextSample();
    s32 GetSample() const;
    void WriteFIFOX_BX(u8 byte);
};

const s8 gbaSquarePattern::m_pattern[4][8] =
{
    {0, 0, 1, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}  // 75%
};

const u32 gbaNoisePattern::m_pattern[2][2] =
{
    {0x3FFF, 0x3F},
    {14, 6}
};

const s32 m_samplerclk = 16777216 / GBA_SAMPLERATE;
gbaSoundChannel1 m_sc1;
gbaSoundChannel2 m_sc2;
gbaSoundChannel3 m_sc3;
gbaSoundChannel4 m_sc4;
gbaDirectSound m_dsA(0x040000A0);
gbaDirectSound m_dsB(0x040000A4);
bool m_masterenable;
s32 m_SOXvol[2];
s32 m_SOXcnt[2];
s32 m_PSGvol;
s32 m_dsAvol;
s32 m_dsBvol;
s32 m_dscnt[2];
s32 m_biaslevel;
t8 m_SOUND1CNT_L;
t16 m_SOUND1CNT_H;
t16 m_SOUND1CNT_X;
t16 m_SOUND2CNT_L;
t16 m_SOUND2CNT_H;
t8 m_SOUND3CNT_L;
t16 m_SOUND3CNT_H;
t16 m_SOUND3CNT_X;
t16 m_SOUND4CNT_L;
t16 m_SOUND4CNT_H;
t16 m_SOUNDCNT_L;
t16 m_SOUNDCNT_H;
t8 m_SOUNDCNT_X;
t16 m_SOUNDBIAS;
s32 m_samplerticks;

gbaSquarePattern::gbaSquarePattern()
{
    Reset();
}

void gbaSquarePattern::DoReloadCounter()
{
    m_ticks += 16 * (2048 - m_freq);
}

void gbaSquarePattern::DoSync()
{
    m_out = (m_out + 1) & 7;
    DoReloadCounter();
}

void gbaSquarePattern::Sync(s32 ticks)
{
    m_ticks -= ticks;
    while (m_ticks <= 0) {DoSync();}
}

void gbaSquarePattern::Reset()
{
    m_dutycycle = m_freq = m_out = m_ticks = 0;
    DoReloadCounter();
}

void gbaSquarePattern::Init()
{
}

s8 gbaSquarePattern::GetSample() const
{
    return m_pattern[m_dutycycle][m_out];
}

void gbaSquarePattern::SetDutyCycle(s32 dutycycle)
{
    m_dutycycle = dutycycle & 3;
}

void gbaSquarePattern::SetFrequency(s32 frequency)
{
    m_freq = frequency & 0x7FF;
}

s32 gbaSquarePattern::GetFrequency()
{
    return m_freq;
}

gbaSweepUnit::gbaSweepUnit(gbaSquarePattern *square)
{
    m_square = square;
    Reset();
}

void gbaSweepUnit::DoReloadCounter()
{
    m_ticks += 131072;
}

void gbaSweepUnit::DoSweepFrequency()
{
    m_nextfreq = m_freq + (m_direction * (m_freq >> m_shifts));
    if (m_nextfreq > 0x7FF) {m_off = true;} else if (m_nextfreq < 0) {m_nextfreq = m_freq;}
}

bool gbaSweepUnit::DoSync()
{
    if (m_counter == 0) {if (m_time == 0) {return false;} else {m_counter = m_time + 1;}}
    m_counter = (m_counter - 1) & 7;
    if (m_counter != 0) {return false;}
    m_counter = m_time;
    if (m_time == 0 || !m_active || m_shifts == 0 || m_off) {return false;}
    DoSweepFrequency();
    if (m_off) {return false;}
    m_freq = m_nextfreq;
    DoSweepFrequency();
    return true;
}

s32 gbaSweepUnit::PartialSync()
{
    s32 ret = m_ticks;
    m_square->Sync(m_ticks);
    if (DoSync()) {m_square->SetFrequency(m_freq);}
    m_ticks = 0;
    DoReloadCounter();
    return ret;
}

void gbaSweepUnit::Sync(s32 ticks)
{
    s32 t = ticks;
    while (m_ticks <= t) {t -= PartialSync();}
    if (t <= 0) {return;}
    m_ticks -= t;
    m_square->Sync(t);
}

void gbaSweepUnit::Reset()
{
    m_time = m_shifts = m_counter = m_freq = m_nextfreq = m_ticks = 0;
    m_direction = 1;
    m_off = true;
    m_active = false;
    DoReloadCounter();
}

void gbaSweepUnit::Init(s32 frequency)
{
    m_freq = frequency & 0x7FF;
    m_counter = m_time;
    m_active = (m_time != 0 || m_shifts != 0) && m_square != 0;
    m_off = false;
    if (m_shifts != 0) {DoSweepFrequency();}
}

s32 gbaSweepUnit::GetFrequency() const
{
    return m_freq;
}

bool gbaSweepUnit::IsChannelOff() const
{
    return m_off;
}

void gbaSweepUnit::SetSweepRegister(u8 NRX0)
{
    m_time = SUBVAL(NRX0, 4, 7);
    m_direction = (NRX0 & BIT(3)) != 0 ? -1 : 1;
    m_shifts = NRX0 & 7;
}

gbaSoundLength::gbaSoundLength()
{
    m_maxlength = 0;
    Reset();
}

void gbaSoundLength::DoReloadCounter()
{
    m_ticks += 65536;
}

void gbaSoundLength::DoSync()
{
    DoReloadCounter();
    if (!m_active || m_counter <= 0) {return;}
    m_counter--;
    if (m_counter == 0) {m_off = true;}
}

void gbaSoundLength::Sync(s32 ticks)
{
    m_ticks -= ticks;
    while (m_ticks <= 0) {DoSync();}
}

void gbaSoundLength::Reset()
{
    m_counter = m_ticks = 0;
    m_active = false;
    m_off = true;
    DoReloadCounter();
}

void gbaSoundLength::Init()
{
    m_off = false;
    if (m_counter == 0) {m_counter = m_maxlength;}
}

bool gbaSoundLength::IsChannelOff() const
{
    return m_off;
}

void gbaSoundLength::SetMaxLength(s32 maxlength)
{
    m_maxlength = maxlength;
}

void gbaSoundLength::SetCounter(s32 counter)
{
    m_counter = counter;
}

void gbaSoundLength::SetEnable(s32 enable)
{
    m_active = enable != 0;
}

gbaVolumeEnvelope::gbaVolumeEnvelope()
{
    Reset();
}

void gbaVolumeEnvelope::DoReloadCounter()
{
    m_ticks += 262144;
}

void gbaVolumeEnvelope::DoStep()
{
    s32 nextvol;
    nextvol = m_vol + m_direction;
    if (nextvol < 0 || nextvol > 15) {m_active = false;} else {m_vol = nextvol;}
}

void gbaVolumeEnvelope::DoSync()
{
    DoReloadCounter();
    if (m_step == 0 || !m_active) {return;}
    m_counter = (m_counter - 1) & 7;
    if (m_counter != 0) {return;}
    m_counter = m_step;
    DoStep();
}

void gbaVolumeEnvelope::Sync(s32 ticks)
{
    m_ticks -= ticks;
    while (m_ticks <= 0) {DoSync();}
}

void gbaVolumeEnvelope::Reset()
{
    m_initvol = m_vol = m_step = m_counter = m_ticks = 0;
    m_direction = -1;
    m_active = false;
    DoReloadCounter();
}

void gbaVolumeEnvelope::Init()
{
    m_vol = m_initvol;
    m_active = true;
    m_counter = m_step;
}

s32 gbaVolumeEnvelope::GetVolume() const
{
    return m_vol;
}

void gbaVolumeEnvelope::SetVolumeEnvelopeRegister(u8 NRX2)
{
    m_initvol = SUBVAL(NRX2, 4, 0x0F);
    m_direction = (NRX2 & BIT(3)) != 0 ? 1 : -1;
    m_step = NRX2 & 7;
}

gbaWavePattern::gbaWavePattern()
{
    Reset(false);
}

void gbaWavePattern::DoReloadCounter()
{
    m_ticks += 8 * (2048 - m_freq);
}

void gbaWavePattern::DoSync()
{
    DoReloadCounter();
    m_position = (m_position + 1) & 0x1F;
    if (m_use2banks && m_position == 0)
    {
        m_playbank = ~m_playbank & 1;
        m_databank = ~m_databank & 1;
    }
    s32 pos = m_position / 2;
    s32 index = (m_position & 1) != 0 ? 0 : 4;
    m_sample = SUBVAL(m_pattern[m_playbank][pos], index, 0x0F);
}

void gbaWavePattern::Sync(s32 ticks)
{
    if (m_off) {return;}
    m_ticks -= ticks;
    while (m_ticks <= 0) {DoSync();}
}

void gbaWavePattern::Reset(bool preservewaveram)
{
    m_position = m_playbank = m_initbank = 0;
    m_databank = 0;
    m_ticks = m_freq = 0;
    m_sample = 0;
    DoReloadCounter();
    m_off = true;
    m_enable = false;
    m_use2banks = false;
}

void gbaWavePattern::Init()
{
    m_position = 0;
    m_ticks = 0;
    DoReloadCounter();
    m_off = !m_enable;
    if (m_use2banks) {m_playbank = m_initbank;} else {m_playbank = 0;}
    if (m_use2banks) {m_databank = ~m_playbank & 1;} else {m_databank = 0;};
    m_sample = SUBVAL(m_pattern[m_playbank][0], 4, 0x0F);
}

s8 gbaWavePattern::GetSample() const
{
    return m_sample;
}

bool gbaWavePattern::IsChannelOff() const
{
    return m_off;
}

void gbaWavePattern::SetOutputEnable(s32 enable)
{
    m_enable = enable != 0;
    if (!m_enable) {m_off = true;}
}

void gbaWavePattern::SetFrequency(s32 frequency)
{
    m_freq = frequency & 0x7FF;
}

void gbaWavePattern::SetBank(s32 bank)
{
    m_initbank = bank & 1;
}

void gbaWavePattern::SetDimension(bool use2banks)
{
    m_use2banks = use2banks;
}

void gbaWavePattern::WriteWaveRAM(u8 offset, s8 samples)
{
    m_pattern[m_databank][offset & 0x0F] = samples;
}

s8 gbaWavePattern::ReadWaveRAM(u8 offset)
{
    return m_pattern[m_databank][offset & 0x0F];
}

gbaNoisePattern::gbaNoisePattern()
{
    Reset();
}

void gbaNoisePattern::DoReloadCounter()
{
    m_ticks += (m_ratio + 1) << (m_shiftclk + 5);
}

void gbaNoisePattern::DoSync()
{
    u32 shift = (m_shiftreg >> 1) & m_pattern[0][m_width];
    m_shiftreg = shift | (((m_shiftreg ^ shift) & 1) << m_pattern[1][m_width]);
    DoReloadCounter();
    m_sample = ~m_shiftreg & 1;
}

void gbaNoisePattern::Sync(s32 ticks)
{
    m_ticks -= ticks;
    while (m_ticks <= 0) {DoSync();}
}

void gbaNoisePattern::Init()
{
    m_shiftreg = 1 << m_pattern[1][m_width];
    m_ticks = 0;
    DoReloadCounter();
}

void gbaNoisePattern::Reset()
{
    m_shiftreg = 1;
    m_ratio = m_shiftclk = m_width = m_ticks = 0;
    DoReloadCounter();
}

s8 gbaNoisePattern::GetSample() const
{
    return m_sample;
}

void gbaNoisePattern::SetNoiseRegister(u8 NR43)
{
    m_shiftclk = SUBVAL(NR43, 4, 0x0F);
    m_width = (NR43 & BIT(3)) != 0 ? 1 : 0;
    m_ratio = NR43 & 7;
}

gbaSoundChannel1::gbaSoundChannel1() : m_sweep(&m_square)
{
    m_length.SetMaxLength(64);
    Reset();
}

void gbaSoundChannel1::Sync(s32 ticks)
{
    m_sweep.Sync(ticks);
    m_length.Sync(ticks);
    m_volume.Sync(ticks);
    m_freq = m_square.GetFrequency();
    if (m_sweep.IsChannelOff() || m_length.IsChannelOff()) {m_off = true;}
}

void gbaSoundChannel1::Reset()
{
    m_off = true;
    m_freq = 0;
    m_square.Reset();
    m_sweep.Reset();
    m_length.Reset();
    m_volume.Reset();
}

s32 gbaSoundChannel1::GetSample() const
{
    return (m_square.GetSample() != 0 ? 1 : -1) * m_volume.GetVolume();
}

bool gbaSoundChannel1::IsChannelOff() const
{
    return m_off;
}

void gbaSoundChannel1::WriteNR10(u8 NR10)
{
    m_sweep.SetSweepRegister(NR10);
}

void gbaSoundChannel1::WriteNR11(u8 NR11)
{
    m_square.SetDutyCycle(SUBVAL(NR11, 6, 3));
    m_length.SetCounter(64 - (NR11 & 63));
}

void gbaSoundChannel1::WriteNR12(u8 NR12)
{
    m_volume.SetVolumeEnvelopeRegister(NR12);
}

void gbaSoundChannel1::WriteNR13(u8 NR13)
{
    m_freq = (m_freq & 0x700) | NR13;
    m_square.SetFrequency(m_freq);
}

void gbaSoundChannel1::WriteNR14(u8 NR14)
{
    m_length.SetEnable(NR14 & BIT(6));
    m_freq = (m_freq & 0xFF) | ((NR14 << 8) & 0x700);
    m_square.SetFrequency(m_freq);
    if ((NR14 & BIT(7)) == 0) {return;}
    m_square.Init();
    m_length.Init();
    m_volume.Init();
    m_sweep.Init(m_freq);
    m_off = m_sweep.IsChannelOff();
}

gbaSoundChannel2::gbaSoundChannel2()
{
    m_length.SetMaxLength(64);
    Reset();
}

void gbaSoundChannel2::Sync(s32 ticks)
{
    m_square.Sync(ticks);
    m_length.Sync(ticks);
    m_volume.Sync(ticks);
    if (m_length.IsChannelOff()) {m_off = true;}
}

void gbaSoundChannel2::Reset()
{
    m_off = true;
    m_freq = 0;
    m_square.Reset();
    m_length.Reset();
    m_volume.Reset();
}

s32 gbaSoundChannel2::GetSample() const
{
    return (m_square.GetSample() != 0 ? 1 : -1) * m_volume.GetVolume();
}

bool gbaSoundChannel2::IsChannelOff() const
{
    return m_off;
}

void gbaSoundChannel2::WriteNR21(u8 NR21)
{
    m_square.SetDutyCycle(SUBVAL(NR21, 6, 3));
    m_length.SetCounter(64 - (NR21 & 63));
}

void gbaSoundChannel2::WriteNR22(u8 NR22)
{
    m_volume.SetVolumeEnvelopeRegister(NR22);
}

void gbaSoundChannel2::WriteNR23(u8 NR23)
{
    m_freq = (m_freq & 0x700) | NR23;
    m_square.SetFrequency(m_freq);
}

void gbaSoundChannel2::WriteNR24(u8 NR24)
{
    m_length.SetEnable(NR24 & BIT(6));
    m_freq = (m_freq & 0xFF) | ((NR24 << 8) & 0x700);
    m_square.SetFrequency(m_freq);
    if ((NR24 & BIT(7)) == 0) {return;}
    m_square.Init();
    m_length.Init();
    m_volume.Init();
    m_off = false;
}

gbaSoundChannel3::gbaSoundChannel3()
{
    m_length.SetMaxLength(256);
    Reset(false);
}

void gbaSoundChannel3::Sync(s32 ticks)
{
    m_wave.Sync(ticks);
    m_length.Sync(ticks);
    if (m_length.IsChannelOff()) {m_off = true;}
}

void gbaSoundChannel3::Reset(bool preservewaveram)
{
    m_freq = m_level = 0;
    m_off = true;
    m_forcevol = false;
    m_wave.Reset(preservewaveram);
    m_length.Reset();
}

s32 gbaSoundChannel3::GetSample() const
{
    s32 sample = (2 * m_wave.GetSample()) - 15 ;
    return m_forcevol ? (sample * 3) / 4 : m_level != 0 ? sample >> (m_level - 1) : 0;
}

bool gbaSoundChannel3::IsChannelOff() const
{
    return m_off;
}

void gbaSoundChannel3::WriteNR30(u8 NR30)
{
    m_wave.SetDimension((NR30 & BIT(5)) != 0);
    m_wave.SetBank((NR30 & BIT(6)) != 0 ? 1 : 0);
    m_wave.SetOutputEnable(NR30 & BIT(7));
    if (m_wave.IsChannelOff()) {m_off = true;}
}

void gbaSoundChannel3::WriteNR31(u8 NR31)
{
    m_length.SetCounter(256 - NR31);
}

void gbaSoundChannel3::WriteNR32(u8 NR32)
{
    m_level = SUBVAL(NR32, 5, 3);
    m_forcevol = (NR32 & BIT(7)) != 0;
}

void gbaSoundChannel3::WriteNR33(u8 NR33)
{
    m_freq = (m_freq & 0x700) | NR33;
    m_wave.SetFrequency(m_freq);
}

void gbaSoundChannel3::WriteNR34(u8 NR34)
{
    m_length.SetEnable(NR34 & BIT(6));
    m_freq = (m_freq & 0xFF) | (((u32)NR34 << 8) & 0x700);
    m_wave.SetFrequency(m_freq);
    if ((NR34 & BIT(7)) == 0) {return;}
    m_length.Init();
    m_wave.Init();
    m_off = false;
}

void gbaSoundChannel3::WriteWavePattern(u8 offset, u8 byte)
{
    m_wave.WriteWaveRAM(offset, (s8)byte);
}

u8 gbaSoundChannel3::ReadWavePattern(u8 offset)
{
    return (u8)m_wave.ReadWaveRAM(offset);
}

gbaSoundChannel4::gbaSoundChannel4()
{
    m_length.SetMaxLength(64);
    Reset();
}

void gbaSoundChannel4::Sync(s32 ticks)
{
    m_noise.Sync(ticks);
    m_length.Sync(ticks);
    m_volume.Sync(ticks);
    if (m_length.IsChannelOff()) {m_off = true;}
}

void gbaSoundChannel4::Reset()
{
    m_length.Reset();
    m_volume.Reset();
    m_noise.Reset();
    m_off = true;
}

s32 gbaSoundChannel4::GetSample() const
{
    return (m_noise.GetSample() != 0 ? 1 : -1) * m_volume.GetVolume();
}

bool gbaSoundChannel4::IsChannelOff() const
{
    return m_off;
}

void gbaSoundChannel4::WriteNR41(u8 NR41)
{
    m_length.SetCounter(64 - (NR41 & 63));
}

void gbaSoundChannel4::WriteNR42(u8 NR42)
{
    m_volume.SetVolumeEnvelopeRegister(NR42);
}

void gbaSoundChannel4::WriteNR43(u8 NR43)
{
    m_noise.SetNoiseRegister(NR43);
}

void gbaSoundChannel4::WriteNR44(u8 NR44)
{
    m_length.SetEnable(NR44 & BIT(6));
    if ((NR44 & BIT(7)) == 0) {return;}
    m_length.Init();
    m_volume.Init();
    m_noise.Init();
    m_off = false;
}

gbaDirectSound::gbaDirectSound(u32 fifo)
{
    m_DMADRQ = fifo;
    Reset();
}

void gbaDirectSound::Reset()
{
    m_sample = 0;
    m_sampleratetimer = gbaControl::IRQ_TIMER0;
    ResetFIFO();
}

void gbaDirectSound::SetTimer(gbaControl::InterruptFlag timerid)
{
    m_sampleratetimer = timerid;
}

gbaControl::InterruptFlag gbaDirectSound::GetTimer() const
{
    return m_sampleratetimer;
}

void gbaDirectSound::ResetFIFO()
{
    while (!m_FIFO.empty()) {m_FIFO.pop();}
}

void gbaDirectSound::SendNextSample()
{
    if (!m_FIFO.empty())
    {
        m_sample = m_FIFO.front();
        m_FIFO.pop();
    }
    else
    {
        m_sample = 0;
    }
    if (m_FIFO.size() <= 16) {gbaDMA::OnFIFORequest(m_DMADRQ);}
}

s32 gbaDirectSound::GetSample() const
{
    return m_sample;
}

void gbaDirectSound::WriteFIFOX_BX(u8 byte)
{
    m_FIFO.push((s8)byte);
}

void SyncPSG(s32 ticks)
{
    if (!m_masterenable) {return;}
    m_sc1.Sync(ticks);
    m_sc2.Sync(ticks);
    m_sc3.Sync(ticks);
    m_sc4.Sync(ticks);
}

void Reset(bool preservewaveram)
{
    m_sc1.Reset();
    m_sc2.Reset();
    m_sc3.Reset(preservewaveram);
    m_sc4.Reset();
    m_dsA.Reset();
    m_dsB.Reset();
    m_SOXvol[0] = m_SOXvol[1] = 0;
    m_SOXcnt[0] = m_SOXcnt[1] = 0;
    m_SOUND1CNT_L.b = 0;
    m_SOUND1CNT_H.w = 0;
    m_SOUND1CNT_X.w = 0;
    m_SOUND2CNT_L.w = 0;
    m_SOUND2CNT_H.w = 0;
    m_SOUND3CNT_L.b = 0;
    m_SOUND3CNT_H.w = 0;
    m_SOUND3CNT_X.w = 0;
    m_SOUND4CNT_L.w = 0;
    m_SOUND4CNT_H.w = 0;
    m_SOUNDCNT_L.w = 0;
}

void Reset()
{
    Reset(false);
    m_PSGvol = 2;
    m_dsAvol = 1;
    m_dsBvol = 1;
    m_dscnt[0] = m_dscnt[1] = 0;
    m_biaslevel = 0x200;
    m_masterenable = true;
    m_SOUNDCNT_H.w = 0;
    m_SOUNDCNT_X.b = BIT(7);
    m_SOUNDBIAS.w = 0x0200;
    m_samplerticks = m_samplerclk;
}

void OnTimerOverflow(gbaControl::InterruptFlag tmr)
{
    if (!m_masterenable) {return;}
    if (m_dsA.GetTimer() == tmr) {m_dsA.SendNextSample();}
    if (m_dsB.GetTimer() == tmr) {m_dsB.SendNextSample();}
}

void GetSampleSO(s32 *m_right, s32 *m_left)
{
    if (!m_masterenable)
    {
        *m_right = *m_left = 0;//m_biaslevel;
        return;
    }
    bool chnon[4] = {!m_sc1.IsChannelOff(), !m_sc2.IsChannelOff(), !m_sc3.IsChannelOff(), !m_sc4.IsChannelOff()};
    s32 sample[4] = {m_sc1.GetSample(), m_sc2.GetSample(), m_sc3.GetSample(), m_sc4.GetSample()};
    s32 psg_r = 0;
    if (chnon[0] && (m_SOXcnt[0] & BIT(0)) != 0) {psg_r += sample[0];}
    if (chnon[1] && (m_SOXcnt[0] & BIT(1)) != 0) {psg_r += sample[1];}
    if (chnon[2] && (m_SOXcnt[0] & BIT(2)) != 0) {psg_r += sample[2];}
    if (chnon[3] && (m_SOXcnt[0] & BIT(3)) != 0) {psg_r += sample[3];}
    psg_r = (psg_r * m_SOXvol[0]) >> m_PSGvol;
    s32 psg_l = 0;
    if (chnon[0] && (m_SOXcnt[1] & BIT(0)) != 0) {psg_l += sample[0];}
    if (chnon[1] && (m_SOXcnt[1] & BIT(1)) != 0) {psg_l += sample[1];}
    if (chnon[2] && (m_SOXcnt[1] & BIT(2)) != 0) {psg_l += sample[2];}
    if (chnon[3] && (m_SOXcnt[1] & BIT(3)) != 0) {psg_l += sample[3];}
    psg_l = (psg_l * m_SOXvol[1]) >> m_PSGvol;
    s32 ds_r = 0;
    if ((m_dscnt[0] & BIT(0)) != 0) {ds_r += m_dsA.GetSample() >> m_dsAvol;}
    if ((m_dscnt[0] & BIT(1)) != 0) {ds_r += m_dsB.GetSample() >> m_dsBvol;}
    //ds_r *= 4;
    ds_r *= 2;
    s32 ds_l = 0;
    if ((m_dscnt[1] & BIT(0)) != 0) {ds_l += m_dsA.GetSample() >> m_dsAvol;}
    if ((m_dscnt[1] & BIT(1)) != 0) {ds_l += m_dsB.GetSample() >> m_dsBvol;}
    //ds_l *= 4;
    ds_l *= 2;
    //s32 r = psg_r + ds_r + m_biaslevel;
    //if (r < 0) {r = 0;} else if (r > 0x3FF) {r = 0x3FF;}
    //s32 l = psg_l + ds_l + m_biaslevel;
    //if (l < 0) {l = 0;} else if (l > 0x3FF) {l = 0x3FF;}
    //*m_right = r - m_biaslevel;
    //*m_left = l - m_biaslevel;
    s32 r = psg_r + ds_r;
    s32 l = psg_l + ds_l;
    *m_right = r;
    *m_left = l;
}

void WriteSOUND1CNT_L(u8 byte)
{
    m_SOUND1CNT_L.b = byte;
    m_sc1.WriteNR10(byte);
}

void WriteSOUND1CNT_H_B0(u8 byte)
{
    m_SOUND1CNT_H.b.b0.b = byte;
    m_sc1.WriteNR11(byte);
}

void WriteSOUND1CNT_H_B1(u8 byte)
{
    m_SOUND1CNT_H.b.b1.b = byte;
    m_sc1.WriteNR12(byte);
}

void WriteSOUND1CNT_X_B0(u8 byte)
{
    m_SOUND1CNT_X.b.b0.b = byte;
    m_sc1.WriteNR13(byte);
}

void WriteSOUND1CNT_X_B1(u8 byte)
{
    m_SOUND1CNT_X.b.b1.b = byte;
    m_sc1.WriteNR14(byte);
}

void WriteSOUND2CNT_L_B0(u8 byte)
{
    m_SOUND2CNT_L.b.b0.b = byte;
    m_sc2.WriteNR21(byte);
}

void WriteSOUND2CNT_L_B1(u8 byte)
{
    m_SOUND2CNT_L.b.b1.b = byte;
    m_sc2.WriteNR22(byte);
}

void WriteSOUND2CNT_H_B0(u8 byte)
{
    m_SOUND2CNT_H.b.b0.b = byte;
    m_sc2.WriteNR23(byte);
}

void WriteSOUND2CNT_H_B1(u8 byte)
{
    m_SOUND2CNT_H.b.b1.b = byte;
    m_sc2.WriteNR24(byte);
}

void WriteSOUND3CNT_L(u8 byte)
{
    m_SOUND3CNT_L.b = byte;
    m_sc3.WriteNR30(byte);
}

void WriteSOUND3CNT_H_B0(u8 byte)
{
    m_SOUND3CNT_H.b.b0.b = byte;
    m_sc3.WriteNR31(byte);
}

void WriteSOUND3CNT_H_B1(u8 byte)
{
    m_SOUND3CNT_H.b.b1.b = byte;
    m_sc3.WriteNR32(byte);
}

void WriteSOUND3CNT_X_B0(u8 byte)
{
    m_SOUND3CNT_X.b.b0.b = byte;
    m_sc3.WriteNR33(byte);
}

void WriteSOUND3CNT_X_B1(u8 byte)
{
    m_SOUND3CNT_X.b.b1.b = byte;
    m_sc3.WriteNR34(byte);
}

void WriteSOUND4CNT_L_B0(u8 byte)
{
    m_SOUND4CNT_L.b.b0.b = byte;
    m_sc4.WriteNR41(byte);
}

void WriteSOUND4CNT_L_B1(u8 byte)
{
    m_SOUND4CNT_L.b.b1.b = byte;
    m_sc4.WriteNR42(byte);
}

void WriteSOUND4CNT_H_B0(u8 byte)
{
    m_SOUND4CNT_H.b.b0.b = byte;
    m_sc4.WriteNR43(byte);
}

void WriteSOUND4CNT_H_B1(u8 byte)
{
    m_SOUND4CNT_H.b.b1.b = byte;
    m_sc4.WriteNR44(byte);
}

void WriteSOUNDCNT_L_B0(u8 byte)
{
    m_SOUNDCNT_L.b.b0.b = byte;
    m_SOXvol[0] = (byte & 7) + 1;
    m_SOXvol[1] = SUBVAL(byte, 4, 7) + 1;
}

void WriteSOUNDCNT_L_B1(u8 byte)
{
    m_SOUNDCNT_L.b.b1.b = byte;
    m_SOXcnt[0] = byte & 0x0F;
    m_SOXcnt[1] = SUBVAL(byte, 4, 0x0F);
}

void WriteSOUNDCNT_H_B0(u8 byte)
{
    m_SOUNDCNT_H.b.b0.b = byte;
    m_PSGvol = 2 - (byte & 3);
    if (m_PSGvol < 0) {m_PSGvol = 0;}
    m_dsAvol = (byte & BIT(2)) != 0 ? 0 : 1;
    m_dsBvol = (byte & BIT(3)) != 0 ? 0 : 1;
}

void WriteSOUNDCNT_H_B1(u8 byte)
{
    m_SOUNDCNT_H.b.b1.b = byte & ~(0x88);
    m_dscnt[0] = ((byte & BIT(0)) != 0 ? BIT(0) : 0) | ((byte & BIT(4)) != 0 ? BIT(1) : 0);
    m_dscnt[1] = ((byte & BIT(1)) != 0 ? BIT(0) : 0) | ((byte & BIT(5)) != 0 ? BIT(1) : 0);
    m_dsA.SetTimer((byte & BIT(2)) != 0 ? gbaControl::IRQ_TIMER1 : gbaControl::IRQ_TIMER0);
    m_dsB.SetTimer((byte & BIT(6)) != 0 ? gbaControl::IRQ_TIMER1 : gbaControl::IRQ_TIMER0);
    if ((byte & BIT(3)) != 0) {m_dsA.ResetFIFO();}
    if ((byte & BIT(7)) != 0) {m_dsB.ResetFIFO();}
}

void WriteSOUNDCNT_X(u8 byte)
{
    m_SOUNDCNT_X.b = byte;
    bool nextmenable = (byte & BIT(7)) != 0;
    if (m_masterenable != nextmenable) {Reset(true);}
    m_masterenable = nextmenable;
}

void WriteSOUNDBIAS_B0(u8 byte)
{
    m_SOUNDBIAS.b.b0.b = byte;
    m_biaslevel = m_SOUNDBIAS.w & 0x3FF;
}

void WriteSOUNDBIAS_B1(u8 byte)
{
    m_SOUNDBIAS.b.b1.b = byte;
    m_biaslevel = m_SOUNDBIAS.w & 0x3FF;
}

u8 ReadSOUNDCNT_X()
{
    if (!m_masterenable) {return 0;}
    u8 f = BIT(7);
    if (!m_sc1.IsChannelOff()) {f |= BIT(0);}
    if (!m_sc2.IsChannelOff()) {f |= BIT(1);}
    if (!m_sc3.IsChannelOff()) {f |= BIT(2);}
    if (!m_sc4.IsChannelOff()) {f |= BIT(3);}
    return f;
}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width)
{
    u8 bytes[4] = {data->w.w0.b.b0.b, data->w.w0.b.b1.b, data->w.w1.b.b0.b, data->w.w1.b.b1.b};
    u32 base = address & ~(width - 1);

    for (u32 i = 0; i < (u32)width; i++)
    {
        switch (base + i)
        {
        case 0x04000060: WriteSOUND1CNT_L(bytes[i]); break;
        case 0x04000062: WriteSOUND1CNT_H_B0(bytes[i]); break;
        case 0x04000063: WriteSOUND1CNT_H_B1(bytes[i]); break;
        case 0x04000064: WriteSOUND1CNT_X_B0(bytes[i]); break;
        case 0x04000065: WriteSOUND1CNT_X_B1(bytes[i]); break;
        case 0x04000068: WriteSOUND2CNT_L_B0(bytes[i]); break;
        case 0x04000069: WriteSOUND2CNT_L_B1(bytes[i]); break;
        case 0x0400006C: WriteSOUND2CNT_H_B0(bytes[i]); break;
        case 0x0400006D: WriteSOUND2CNT_H_B1(bytes[i]); break;
        case 0x04000070: WriteSOUND3CNT_L(bytes[i]); break;
        case 0x04000072: WriteSOUND3CNT_H_B0(bytes[i]); break;
        case 0x04000073: WriteSOUND3CNT_H_B1(bytes[i]); break;
        case 0x04000074: WriteSOUND3CNT_X_B0(bytes[i]); break;
        case 0x04000075: WriteSOUND3CNT_X_B1(bytes[i]); break;
        case 0x04000078: WriteSOUND4CNT_L_B0(bytes[i]); break;
        case 0x04000079: WriteSOUND4CNT_L_B1(bytes[i]); break;
        case 0x0400007C: WriteSOUND4CNT_H_B0(bytes[i]); break;
        case 0x0400007D: WriteSOUND4CNT_H_B1(bytes[i]); break;
        case 0x04000080: WriteSOUNDCNT_L_B0(bytes[i]); break;
        case 0x04000081: WriteSOUNDCNT_L_B1(bytes[i]); break;
        case 0x04000082: WriteSOUNDCNT_H_B0(bytes[i]); break;
        case 0x04000083: WriteSOUNDCNT_H_B1(bytes[i]); break;
        case 0x04000084: WriteSOUNDCNT_X(bytes[i]); break;
        case 0x04000088: WriteSOUNDBIAS_B0(bytes[i]); break;
        case 0x04000089: WriteSOUNDBIAS_B1(bytes[i]); break;
        case 0x04000090: m_sc3.WriteWavePattern(0x00, bytes[i]); break;
        case 0x04000091: m_sc3.WriteWavePattern(0x01, bytes[i]); break;
        case 0x04000092: m_sc3.WriteWavePattern(0x02, bytes[i]); break;
        case 0x04000093: m_sc3.WriteWavePattern(0x03, bytes[i]); break;
        case 0x04000094: m_sc3.WriteWavePattern(0x04, bytes[i]); break;
        case 0x04000095: m_sc3.WriteWavePattern(0x05, bytes[i]); break;
        case 0x04000096: m_sc3.WriteWavePattern(0x06, bytes[i]); break;
        case 0x04000097: m_sc3.WriteWavePattern(0x07, bytes[i]); break;
        case 0x04000098: m_sc3.WriteWavePattern(0x08, bytes[i]); break;
        case 0x04000099: m_sc3.WriteWavePattern(0x09, bytes[i]); break;
        case 0x0400009A: m_sc3.WriteWavePattern(0x0A, bytes[i]); break;
        case 0x0400009B: m_sc3.WriteWavePattern(0x0B, bytes[i]); break;
        case 0x0400009C: m_sc3.WriteWavePattern(0x0C, bytes[i]); break;
        case 0x0400009D: m_sc3.WriteWavePattern(0x0D, bytes[i]); break;
        case 0x0400009E: m_sc3.WriteWavePattern(0x0E, bytes[i]); break;
        case 0x0400009F: m_sc3.WriteWavePattern(0x0F, bytes[i]); break;
        case 0x040000A0: m_dsA.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A1: m_dsA.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A2: m_dsA.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A3: m_dsA.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A4: m_dsB.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A5: m_dsB.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A6: m_dsB.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        case 0x040000A7: m_dsB.WriteFIFOX_BX(bytes[i]); if (width != 4) {Emulator::LogMessage("wtf");} break;
        }
    }
}

void ReadIO(u32 address, t32 *data, gbaMemory::DataType width)
{
    u8 *bytes[4] = {&data->w.w0.b.b0.b, &data->w.w0.b.b1.b, &data->w.w1.b.b0.b, &data->w.w1.b.b1.b};
    u32 base = address & ~(width - 1);

    for (u32 i = 0; i < (u32)width; i++)
    {
        switch (base + i)
        {
        case 0x04000060: *bytes[i] = m_SOUND1CNT_L.b; break;
        case 0x04000061: *bytes[i] = 0; break;
        case 0x04000062: *bytes[i] = m_SOUND1CNT_H.b.b0.b; break;
        case 0x04000063: *bytes[i] = m_SOUND1CNT_H.b.b1.b; break;
        case 0x04000064: *bytes[i] = m_SOUND1CNT_X.b.b0.b; break;
        case 0x04000065: *bytes[i] = m_SOUND1CNT_X.b.b1.b; break;
        case 0x04000066: *bytes[i] = 0; break;
        case 0x04000067: *bytes[i] = 0; break;
        case 0x04000068: *bytes[i] = m_SOUND2CNT_L.b.b0.b; break;
        case 0x04000069: *bytes[i] = m_SOUND2CNT_L.b.b1.b; break;
        case 0x0400006A: *bytes[i] = 0; break;
        case 0x0400006B: *bytes[i] = 0; break;
        case 0x0400006C: *bytes[i] = m_SOUND2CNT_H.b.b0.b; break;
        case 0x0400006D: *bytes[i] = m_SOUND2CNT_H.b.b1.b; break;
        case 0x0400006E: *bytes[i] = 0; break;
        case 0x0400006F: *bytes[i] = 0; break;
        case 0x04000070: *bytes[i] = m_SOUND3CNT_L.b; break;
        case 0x04000071: *bytes[i] = 0; break;
        case 0x04000072: *bytes[i] = m_SOUND3CNT_H.b.b0.b; break;
        case 0x04000073: *bytes[i] = m_SOUND3CNT_H.b.b1.b; break;
        case 0x04000074: *bytes[i] = m_SOUND3CNT_X.b.b0.b; break;
        case 0x04000075: *bytes[i] = m_SOUND3CNT_X.b.b1.b; break;
        case 0x04000076: *bytes[i] = 0; break;
        case 0x04000077: *bytes[i] = 0; break;
        case 0x04000078: *bytes[i] = m_SOUND4CNT_L.b.b0.b; break;
        case 0x04000079: *bytes[i] = m_SOUND4CNT_L.b.b1.b; break;
        case 0x0400007A: *bytes[i] = 0; break;
        case 0x0400007B: *bytes[i] = 0; break;
        case 0x0400007C: *bytes[i] = m_SOUND4CNT_H.b.b0.b; break;
        case 0x0400007D: *bytes[i] = m_SOUND4CNT_H.b.b1.b; break;
        case 0x0400007E: *bytes[i] = 0; break;
        case 0x0400007F: *bytes[i] = 0; break;
        case 0x04000080: *bytes[i] = m_SOUNDCNT_L.b.b0.b; break;
        case 0x04000081: *bytes[i] = m_SOUNDCNT_L.b.b1.b; break;
        case 0x04000082: *bytes[i] = m_SOUNDCNT_H.b.b0.b; break;
        case 0x04000083: *bytes[i] = m_SOUNDCNT_H.b.b1.b; break;
        case 0x04000084: *bytes[i] = ReadSOUNDCNT_X(); break;
        case 0x04000085: *bytes[i] = 0; break;
        case 0x04000086: *bytes[i] = 0; break;
        case 0x04000087: *bytes[i] = 0; break;
        case 0x04000088: *bytes[i] = m_SOUNDBIAS.b.b0.b; break;
        case 0x04000089: *bytes[i] = m_SOUNDBIAS.b.b1.b; break;
        case 0x0400008A: *bytes[i] = 0; break;
        case 0x0400008B: *bytes[i] = 0; break;
        case 0x04000090: *bytes[i] = m_sc3.ReadWavePattern(0x00); break;
        case 0x04000091: *bytes[i] = m_sc3.ReadWavePattern(0x01); break;
        case 0x04000092: *bytes[i] = m_sc3.ReadWavePattern(0x02); break;
        case 0x04000093: *bytes[i] = m_sc3.ReadWavePattern(0x03); break;
        case 0x04000094: *bytes[i] = m_sc3.ReadWavePattern(0x04); break;
        case 0x04000095: *bytes[i] = m_sc3.ReadWavePattern(0x05); break;
        case 0x04000096: *bytes[i] = m_sc3.ReadWavePattern(0x06); break;
        case 0x04000097: *bytes[i] = m_sc3.ReadWavePattern(0x07); break;
        case 0x04000098: *bytes[i] = m_sc3.ReadWavePattern(0x08); break;
        case 0x04000099: *bytes[i] = m_sc3.ReadWavePattern(0x09); break;
        case 0x0400009A: *bytes[i] = m_sc3.ReadWavePattern(0x0A); break;
        case 0x0400009B: *bytes[i] = m_sc3.ReadWavePattern(0x0B); break;
        case 0x0400009C: *bytes[i] = m_sc3.ReadWavePattern(0x0C); break;
        case 0x0400009D: *bytes[i] = m_sc3.ReadWavePattern(0x0D); break;
        case 0x0400009E: *bytes[i] = m_sc3.ReadWavePattern(0x0E); break;
        case 0x0400009F: *bytes[i] = m_sc3.ReadWavePattern(0x0F); break;
        }
    }
}

s32 PartialSync()
{
    s32 ret = m_samplerticks;
    SyncPSG(m_samplerticks);
    s32 r = 0;
    s32 l = 0;
    GetSampleSO(&r, &l);
    Emulator::SendSoundSample(l, r);
    m_samplerticks = m_samplerclk;
    return ret;
}

void Sync(s32 ticks)
{
    s32 t = ticks;
    while (m_samplerticks <= t) {t -= PartialSync();}
    if (t <= 0) {return;}
    m_samplerticks -= t;
    SyncPSG(t);
}

s32 GetNextEvent()
{
    return m_samplerticks;
}
}
