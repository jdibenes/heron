//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include "gba_memory.h"
#include "gba_cpu.h"
#include "../emulator.h"

namespace gbaCPU
{
enum Exception
{
    EXCEPTION_RESET,
    EXCEPTION_DATAABORT,
    EXCEPTION_FIQ,
    EXCEPTION_IRQ,
    EXCEPTION_PREFETCHABORT,
    EXCEPTION_SOFTWAREINTERRUPT,
    EXCEPTION_UNDEFINEDINSTRUCTION
};

enum ExceptionVector
{
    VECTOR_BASE                 = 0x00000000,
    VECTOR_RESET                = VECTOR_BASE + 0x00,
    VECTOR_UNDEFINEDINSTRUCTION = VECTOR_BASE + 0x04,
    VECTOR_SOFTWAREINTERRUPT    = VECTOR_BASE + 0x08,
    VECTOR_PREFETCHABORT        = VECTOR_BASE + 0x0C,
    VECTOR_DATAABORT            = VECTOR_BASE + 0x10,
    VECTOR_IRQ                  = VECTOR_BASE + 0x18,
    VECTOR_FIQ                  = VECTOR_BASE + 0x1C
};

enum OperatingMode
{
    MODE_USER       = 16,
    MODE_FIQ        = 17,
    MODE_IRQ        = 18,
    MODE_SUPERVISOR = 19,
    MODE_ABORT      = 23,
    MODE_UNDEFINED  = 27,
    MODE_SYSTEM     = 31,
    MODE_BITS       = 0x1F
};

enum StatusFlag
{
    FLAG_N = BIT(31),
    FLAG_Z = BIT(30),
    FLAG_C = BIT(29),
    FLAG_V = BIT(28),
    FLAG_I = BIT(7),
    FLAG_F = BIT(6),
    FLAG_T = BIT(5)
};

enum SpecialRegister
{
    REGISTER_SP = 13,
    REGISTER_LR = 14,
    REGISTER_PC = 15
};

enum Coprocessor
{
    COPROCESSOR_14 = 14
};

const bool truthtable[16][16] =
{
//   Valor de CPSR bits 31 a 28 ->                    // | Valor de condicion bits 3 a 0 
//   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   // V
    {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1}, // 0 EQ Z=1         igual o cero
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0}, // 1 NE Z=0         diferente o no cero
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}, // 2 CS C=1         mayor o igual que (unsigned)
    {1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0}, // 3 CC C=0         menor que (unsigned)
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}, // 4 MI N=1         negativo
    {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // 5 PL N=0         positivo o cero
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}, // 6 VS V=1         overflow
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}, // 7 VC V=0         no overflow
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0}, // 8 HI C=1 AND Z=0 mayor que (unsigned)
    {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}, // 9 LS C=0 OR Z=1  menor o igual que (unsigned)
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1}, // A GE N=V         mayor o igual que (signed)
    {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0}, // B LT N!=V        menor que (signed)
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0}, // C GT Z=0 AND N=V mayor que (signed)
    {0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1}, // D LE Z=1 OR N!=V menor o igual que (signed)
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // E AL 1           siempre
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}  // F -- X           reservado
};

s32 (*DecodeAndExecute)();

t32  opcode[3];
s32  N_cycle, S_cycle;
bool exceptionlock;

gbaMemory::DataType instructionlength;

t8 * const prefetch32[4] = {&opcode[2].w.w0.b.b0, &opcode[2].w.w0.b.b1, &opcode[2].w.w1.b.b0, &opcode[2].w.w1.b.b1};
t8 * const prefetch16[4] = {&opcode[2].w.w0.b.b0, &opcode[2].w.w0.b.b1, &opcode[2].w.w0.b.b0, &opcode[2].w.w0.b.b1};

t8 * const *prefetch;

t32 R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, CPSR, SPSR;
t32 R8_fiq, R9_fiq, R10_fiq, R11_fiq, R12_fiq;
t32 R13_fiq, R14_fiq, SPSR_fiq;
t32 R13_svc, R14_svc, SPSR_svc;
t32 R13_abt, R14_abt, SPSR_abt;
t32 R13_irq, R14_irq, SPSR_irq;
t32 R13_und, R14_und, SPSR_und;

t32 * const RX[16]     = {&R0, &R1, &R2, &R3, &R4, &R5, &R6, &R7, &R8,     &R9,     &R10,     &R11,     &R12,     &R13,     &R14,     &R15};
t32 * const RX_fiq[16] = {&R0, &R1, &R2, &R3, &R4, &R5, &R6, &R7, &R8_fiq, &R9_fiq, &R10_fiq, &R11_fiq, &R12_fiq, &R13_fiq, &R14_fiq, &R15};
t32 * const RX_svc[16] = {&R0, &R1, &R2, &R3, &R4, &R5, &R6, &R7, &R8,     &R9,     &R10,     &R11,     &R12,     &R13_svc, &R14_svc, &R15};
t32 * const RX_abt[16] = {&R0, &R1, &R2, &R3, &R4, &R5, &R6, &R7, &R8,     &R9,     &R10,     &R11,     &R12,     &R13_abt, &R14_abt, &R15};
t32 * const RX_irq[16] = {&R0, &R1, &R2, &R3, &R4, &R5, &R6, &R7, &R8,     &R9,     &R10,     &R11,     &R12,     &R13_irq, &R14_irq, &R15};
t32 * const RX_und[16] = {&R0, &R1, &R2, &R3, &R4, &R5, &R6, &R7, &R8,     &R9,     &R10,     &R11,     &R12,     &R13_und, &R14_und, &R15};

t32 * const *RX_xxx;
t32         *SPSR_xxx;

s32   ARM_DecodeAndExecute();
s32 THUMB_DecodeAndExecute();

#define BIC_CPSR(f)          (CPSR.d & ~(f))

#define SET_N(r)             (BITTEST(r, 31)                            ? FLAG_N : 0)
#define SET_Z(r)             (((r) == 0)                                ? FLAG_Z : 0)
#define SET_C(b)             ((b)                                       ? FLAG_C : 0)
#define SET_V_ADD(o1, o2, r) (BITTEST(((o1) ^ (r)) & ( (o2) ^ (r)), 31) ? FLAG_V : 0)
#define SET_V_SUB(o1, o2, r) (BITTEST(((o1) ^ (r)) & (~(o2) ^ (r)), 31) ? FLAG_V : 0)

void EnterOperatingMode(u32 mode)
{
    switch (mode)
    {
    case MODE_USER:
    case MODE_SYSTEM:     RX_xxx = RX;     SPSR_xxx = &SPSR;     break;
    case MODE_FIQ:        RX_xxx = RX_fiq; SPSR_xxx = &SPSR_fiq; break;
    case MODE_IRQ:        RX_xxx = RX_irq; SPSR_xxx = &SPSR_irq; break;
    case MODE_SUPERVISOR: RX_xxx = RX_svc; SPSR_xxx = &SPSR_svc; break;
    case MODE_ABORT:      RX_xxx = RX_abt; SPSR_xxx = &SPSR_abt; break;
    case MODE_UNDEFINED:  RX_xxx = RX_und; SPSR_xxx = &SPSR_und; break;
    default: return;
    }

    CPSR.d = (CPSR.d & ~MODE_BITS) | mode;
}

void EnterOperatingState(u32 tbit)
{
    if (tbit != 0)
    {
        CPSR.d           |=  FLAG_T;
        instructionlength =  gbaMemory::TYPE_HALFWORD;
        prefetch          =  prefetch16;
        DecodeAndExecute  = &THUMB_DecodeAndExecute;
    }
    else
    {
        CPSR.d           &= ~FLAG_T;
        instructionlength =  gbaMemory::TYPE_WORD;
        prefetch          =  prefetch32;
        DecodeAndExecute  = &ARM_DecodeAndExecute;
    }
}

s32 BranchAbsolute(u32 address, bool bx, bool bl, u32 link)
{
    u32 target;
    s32 N[2];
    s32 S[2];

    if (bx) {EnterOperatingState(address & BIT(0));}
    if (bl) {RX_xxx[REGISTER_LR]->d = link;}   

    target = address & ~(instructionlength - 1);
    R15.d  = target + instructionlength;

    gbaMemory::Read(target, &opcode[1], instructionlength, &N[0], &S[0]);
    gbaMemory::Read(R15.d,  &opcode[2], instructionlength, &N[1], &S[1]);

    return N[0] + S[1];
}

u32 GetNextPC()
{
    return R15.d - instructionlength;
}

bool IsOpcodeFetch(t32 const *data) {
    return data == &opcode[0] || data == &opcode[1] || data == &opcode[2];
}

u32 GetPrefetch()
{
    t32 p;
    p.w.w0.b.b0.b = prefetch[0]->b;
    p.w.w0.b.b1.b = prefetch[1]->b;
    p.w.w1.b.b0.b = prefetch[2]->b;
    p.w.w1.b.b1.b = prefetch[3]->b;
    return p.d;
}

s32 EnterException(Exception id)
{
    u32 vector;
    u32 link;
    u32 spsr;
    OperatingMode mode;
    s32 I;

    spsr = CPSR.d;

    switch (id)
    {
    case EXCEPTION_RESET:                vector = VECTOR_RESET;                link = GetNextPC();     mode = MODE_SUPERVISOR; CPSR.d |= FLAG_F; I = 0; break;
    case EXCEPTION_IRQ:                  vector = VECTOR_IRQ;                  link = GetNextPC() + 4; mode = MODE_IRQ;                          I = 4; break;
    case EXCEPTION_SOFTWAREINTERRUPT:    vector = VECTOR_SOFTWAREINTERRUPT;    link = GetNextPC();     mode = MODE_SUPERVISOR;                   I = 0; break;
    case EXCEPTION_UNDEFINEDINSTRUCTION: vector = VECTOR_UNDEFINEDINSTRUCTION; link = GetNextPC();     mode = MODE_UNDEFINED;                    I = 1; break;
    default: return 0;
    }

    EnterOperatingMode(mode);
    EnterOperatingState(0);

    CPSR.d |= FLAG_I;
    SPSR_xxx->d = spsr;

    return BranchAbsolute(vector, false, true, link) + I;
}

bool TestCondition(u32 cc) 
{
    return truthtable[cc][SUBVAL(CPSR.d, 28, 0xF)];
}

s32 SingleStep()
{
    exceptionlock = false;

    opcode[0] = opcode[1];
    opcode[1] = opcode[2];

    R15.d += instructionlength;
    gbaMemory::Read(R15.d, &opcode[2], instructionlength, &N_cycle, &S_cycle);

    return DecodeAndExecute();
}

s32 RequestInterrupt()
{
    return ((CPSR.d & FLAG_I) != 0 || exceptionlock) ? 0 : EnterException(EXCEPTION_IRQ);
}

s32 Reset()
{
    R0.d = R1.d = R2.d = R3.d = R4.d = R5.d = R6.d = R7.d = R8.d = R9.d = R10.d = R11.d = R12.d = R13.d = R14.d = R15.d = CPSR.d = SPSR.d = 0;
    R8_fiq.d = R9_fiq.d = R10_fiq.d = R11_fiq.d = R12_fiq.d = 0;
    R13_fiq.d = R14_fiq.d = SPSR_fiq.d = 0;
    R13_svc.d = R14_svc.d = SPSR_svc.d = 0;
    R13_abt.d = R14_abt.d = SPSR_abt.d = 0;
    R13_irq.d = R14_irq.d = SPSR_irq.d = 0;
    R13_und.d = R14_und.d = SPSR_und.d = 0;

    exceptionlock = false;

    // Fast boot?
    /*
    t32 data;
    data.d = 1;
    s32 N, S;
    gbaMemory::Write(0x04000300, &data, gbaMemory::TYPE_BYTE, &N, &S);
    data.d = 0x8000;
    gbaMemory::Write(0x04000134, &data, gbaMemory::TYPE_HALFWORD, &N, &S);
    data.d = 0;
    gbaMemory::Write(0x04000128, &data, gbaMemory::TYPE_HALFWORD, &N, &S);
    
    for (u32 address = 0x03007E00; address < 0x03008000; address += 4) {
        gbaMemory::Write(address, &data, gbaMemory::TYPE_WORD, &N, &S);
    }


    R13.d = 0x03007F00;
    R13_irq.d = 0x03007FA0;
    R13_svc.d = 0x03007FE0;
    EnterOperatingMode(MODE_SYSTEM);
    EnterOperatingState(0);
    return BranchAbsolute(0x08000000, false, false, 0);
    //*/
    return EnterException(EXCEPTION_RESET);
}

void WriteCPSR()
{
    EnterOperatingMode(CPSR.d & MODE_BITS);
    EnterOperatingState(CPSR.d & FLAG_T);
}

void RestoreCPSR()
{
    CPSR.d = SPSR_xxx->d;
    WriteCPSR();
}

s32 WritePC(bool s)
{
    if (s) {RestoreCPSR();}
    return BranchAbsolute(R15.d, false, false, 0);
}

u32 CountLeadingZerosOrOnes(u32 value, bool ones)
{
    return 0; // TODO
}

s32 MultiplierArrayCycles(u32 rs, bool sign)
{
    u32 lz = CountLeadingZerosOrOnes(rs, false);
    u32 az = lz / 8;
    if (az > 3) {az = 3;}
    u32 r1 = 4 - az;

    if (!sign) {return r1;}

    u32 lo = CountLeadingZerosOrOnes(rs, true);
    u32 ao = lo / 8;
    if (ao > 3) {ao = 3;}
    u32 r2 = 4 - ao;
    return (r1 < r2) ? r1 : r2;

    //return 3;
}

u32 LSL(u32 rs, u32 rn, bool s)
{
    u32 ret = (rn < 32) ? (rs << rn) : 0;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C) | SET_N(ret) | SET_Z(ret) | ((rn == 0) ? (CPSR.d & FLAG_C) : SET_C((rn <= 32) ? BITTEST(rs, 32 - rn) : false));}
    return ret;
}

u32 LSR(u32 rs, u32 rn, bool s)
{
    u32 ret = (rn < 32) ? (rs >> rn) : 0;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C) | SET_N(ret) | SET_Z(ret) | ((rn == 0) ? (CPSR.d & FLAG_C) : SET_C((rn <= 32) ? BITTEST(rs, rn -  1) : false));}
    return ret;
}

u32 ASR(s32 rs, u32 rn, bool s)
{
    u32 ret = (rn < 32) ? (rs >> rn) : (rs >> 31);
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C) | SET_N(ret) | SET_Z(ret) | ((rn == 0) ? (CPSR.d & FLAG_C) : SET_C((rn <= 32) ? BITTEST(rs, rn - 1) : BITTEST(rs, 31)));}
    return ret;
}

u32 ROR(u32 rs, u32 rn, bool s)
{
    u32 sh  = rn & 0x1F;
    u32 ret = (rs >> sh) | (rs << (32 - sh));
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C) | SET_N(ret) | SET_Z(ret) | ((rn == 0) ? (CPSR.d & FLAG_C) : SET_C(BITTEST(ret, 31)));}
    return ret;    
}

u32 RRX(u32 rs, bool s)
{
    u32 ret = (rs >> 1) | ((CPSR.d & FLAG_C) != 0 ? BIT(31) : 0);
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C) | SET_N(ret) | SET_Z(ret) | SET_C(BITTEST(rs, 0));}
    return ret;
}

u32 AND(u32 rs, u32 rn, bool s)
{    
    u32 ret = rs & rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;    
}

u32 EOR(u32 rs, u32 rn, bool s)
{
    u32 ret = rs ^ rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u32 ORR(u32 rs, u32 rn, bool s)
{
    u32 ret = rs | rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u32 BIC(u32 rs, u32 rn, bool s)
{
    u32 ret = rs & ~rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u32 ADD(u32 rs, u32 rn, bool s)
{
    u32 ret = rs + rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C | FLAG_V) | SET_N(ret) | SET_Z(ret) | SET_C(ret < rs) | SET_V_ADD(rs, rn, ret);}
    return ret;
}

u32 ADC(u32 rs, u32 rn, bool s)
{
    u32 par = rn + (((CPSR.d & FLAG_C) != 0) ? 1 : 0);
    u32 ret = rs + par;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C | FLAG_V) | SET_N(ret) | SET_Z(ret) | SET_C((par < rn) || (ret < rs)) | SET_V_ADD(rs, rn, ret);}
    return ret;
}

u32 SUB(u32 rs, u32 rn, bool s)
{
    u32 ret = rs - rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C | FLAG_V) | SET_N(ret) | SET_Z(ret) | SET_C(rn <= rs) | SET_V_SUB(rs, rn, ret);}
    return ret;
}

u32 SBC(u32 rs, u32 rn, bool s)
{
    u32 par = rs - (((CPSR.d & FLAG_C) != 0) ? 0 : 1);
    u32 ret = par - rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z | FLAG_C | FLAG_V) | SET_N(ret) | SET_Z(ret) | SET_C((par <= rs) && (ret <= par)) | SET_V_SUB(rs, rn, ret);}
    return ret;
}

u32 MOV(u32 rs, bool s)
{
    u32 ret = rs;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u32 MVN(u32 rs, bool s)
{
    u32 ret = ~rs;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u32 MUL(u32 rm, u32 rs, bool s)
{
    u32 ret = rm * rs;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u32 MLA(u32 rm, u32 rs, u32 rn, bool s)
{
    u32 ret = (rm * rs) + rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret) | SET_Z(ret);}
    return ret;
}

u64 UMULL(u32 rm, u32 rs, bool s)
{
    t64 ret;
    ret.q = (u64)rm * (u64)rs;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret.d.d1.d) | SET_Z(ret.q);}
    return ret.q;
}

u64 UMLAL(u32 rm, u32 rs, u64 rn, bool s)
{
    t64 ret;
    ret.q = ((u64)rm * (u64)rs) + rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret.d.d1.d) | SET_Z(ret.q);}
    return ret.q;
}

u64 SMULL(s32 rm, s32 rs, bool s)
{
    t64 ret;
    ret.l = (s64)rm * (s64)rs;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret.d.d1.d) | SET_Z(ret.q);}
    return ret.l;
}

u64 SMLAL(s32 rm, s32 rs, s64 rn, bool s)
{
    t64 ret;
    ret.l = ((s64)rm * (s64)rs) + rn;
    if (s) {CPSR.d = BIC_CPSR(FLAG_N | FLAG_Z) | SET_N(ret.d.d1.d) | SET_Z(ret.q);}
    return ret.l;
}

void STR(u32 address, t32 const *data, gbaMemory::DataType width, s32 *N_access, s32 *S_access)
{
    u32 base = address & ~(width - 1);
    gbaMemory::Write(base, data, width, N_access, S_access);
}

void LDR(u32 address, t32 *data, gbaMemory::DataType width, s32 *N_access, s32 *S_access, bool ror, bool sx)
{
    static const u32 mask[3] = {0x000000FF, 0x0000FFFF, 0xFFFFFFFF};
    u32 align      = width - 1;
    u32 misaligned = address &  align;
    u32 base       = address & ~align;

    data->d = 0;
    gbaMemory::Read(base, data, width, N_access, S_access);
    data->d &= mask[width >> 1];

    if (ror && (misaligned != 0))
    {
        data->d = ROR(data->d, misaligned * 8, false);
        if (width == gbaMemory::TYPE_HALFWORD) {width = gbaMemory::TYPE_BYTE;}
    }

    if (sx && (width != gbaMemory::TYPE_WORD)) {data->d = (width == gbaMemory::TYPE_HALFWORD) ? SIGNEX(data->d, 15) : SIGNEX(data->d, 7);}
}

u32 ShiftGroup(u32 op, u32 value, u32 sh, bool s, bool imm)
{
    bool special = imm && (sh == 0);
    u32  ret;

    switch (op)
    {
    case 0:  ret = LSL(value,                sh, s); break;
    case 1:  ret = LSR(value, special ? 32 : sh, s); break;
    case 2:  ret = ASR(value, special ? 32 : sh, s); break;
    default: ret = special ? RRX(value, s) : ROR(value, sh, s);
    }

    return ret;
}
    
s32 SingleDataTransfer(bool load, u32 address, u32 rd, gbaMemory::DataType width, bool ror, bool sx)
{
    s32 N;
    s32 S;
    s32 NSI;

    if (load)
    {
        LDR(address, RX_xxx[rd], width, &N, &S, ror, sx);
        NSI = S_cycle + N + 1;
        if (rd == REGISTER_PC) {NSI += WritePC(false);}
    }
    else
    {
        RX_xxx[REGISTER_PC]->d += instructionlength;
        STR(address, RX_xxx[rd], width, &N, &S);
        RX_xxx[REGISTER_PC]->d -= instructionlength;
        NSI = N_cycle + N;
    }

    return NSI;
}

s32 SingleDataTransfer(bool load, u32 rn, u32 offset, u32 rd, gbaMemory::DataType width, bool ror, bool sx, bool p, bool u, bool w)
{
    u32 final   = RX_xxx[rn]->d + (u ? offset : NEGATE(offset));
    u32 address = p ? final : RX_xxx[rn]->d;
    s32 NSI     = SingleDataTransfer(load, address, rd, width, ror, sx);

    if (w || !p) {RX_xxx[rn]->d = final;}
    return NSI;
}

s32 MultipleDataTransfer(bool load, u32 rlist, u32 rn, bool p, bool u, bool w, bool s)
{
    gbaMemory::DataType width = gbaMemory::TYPE_WORD;
    
    u32 words = 0;
    for (u32 i = 0; i < 16; i++) {if (BITTEST(rlist, i)) {words++;}}
    
    bool empty  = words == 0;
    u32  offset = (!empty ? words : 16) * width;
    u32  start;
    u32  final;

    if (empty) {rlist |= BIT(REGISTER_PC);}

    if (u)
    {
        final = RX_xxx[rn]->d + offset;
        start = RX_xxx[rn]->d;
        if ( p) {start += width;}
    }
    else
    {
        final = RX_xxx[rn]->d - offset;
        start = final;
        if (!p) {start += width;}
    }

    t32 * const *RX_map = s && (!load || !BITTEST(rlist, REGISTER_PC)) ? RX : RX_xxx;

    u32  address    = start;
    bool sequential = false;
    s32  NSI        = load ? (S_cycle + 1) : N_cycle;
    s32  N;
    s32  S;

    if (!load) {RX_xxx[REGISTER_PC]->d += instructionlength;} else if (BITTEST(rlist, rn)) {w = false;}

    for (u32 i = 0; i < 16; i++)
    {
        if (BITTEST(rlist, i))
        {
            if (load) {LDR(address, RX_map[i], width, &N, &S, false, false);} else {STR(address, RX_map[i], width, &N, &S);}
    
            NSI += sequential ? S : N;
            NSI += S;
            sequential = true;
            address += width;
        }
    }

    if (w) {RX_xxx[rn]->d = final;}

    if (!load) {RX_xxx[REGISTER_PC]->d -= instructionlength;} else if (BITTEST(rlist, REGISTER_PC)) {NSI += WritePC(s);}
    return NSI;
}

//-------------------------------------------------------------------------------------------------
// Set de instrucciones ARM (32 bits)
//-------------------------------------------------------------------------------------------------
// Formato 3: Branch and Exchange (BX) ------------------------------------------------------------
s32 ARM_Format3()
{
    u32 rn = SUBVAL(opcode->d, 0, 0xF);
    return S_cycle + BranchAbsolute(RX_xxx[rn]->d, true, false, 0);
}
//-------------------------------------------------------------------------------------------------
// Formato 4: Branch and Branch with Link (B, BL) -------------------------------------------------
s32 ARM_Format4()
{
    u32  offset = SUBVAL(opcode->d, 0, 0xFFFFFF);
    u32  target = RX_xxx[REGISTER_PC]->d + (SIGNEX(offset, 23) << 2);
    bool bl     = BITTEST(opcode->d, 24);

    return S_cycle + BranchAbsolute(target, false, bl, GetNextPC());
}
//-------------------------------------------------------------------------------------------------
// Formato 5: Data Processing ---------------------------------------------------------------------
s32 ARM_Format5() 
{
    static const bool write[16] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1};
    bool s   = BITTEST(opcode->d, 20);
    s32  NSI = 0;
    bool usePC12;
    u32  op2;

    u32 old_cpsr = CPSR.d;

    if (!BITTEST(opcode->d, 25))
    {
        u32 sh;
        usePC12 = BITTEST(opcode->d, 4);
    
        if (usePC12)
        {
            u32 rs = SUBVAL(opcode->d, 8, 0xF);
            RX_xxx[REGISTER_PC]->d += instructionlength;
            sh = RX_xxx[rs]->d & 0xFF;
            NSI += 1;
        }
        else
        {
            sh = SUBVAL(opcode->d, 7, 0x1F);
        }
        
        u32 rm = SUBVAL(opcode->d, 0, 0xF);
        u32 op = SUBVAL(opcode->d, 5, 0x3);
        op2 = ShiftGroup(op, RX_xxx[rm]->d, sh, s, !usePC12);
    }
    else
    {
        usePC12 = false;
        u32 imm = SUBVAL(opcode->d, 0, 0xFF);
        u32 ror = SUBVAL(opcode->d, 8, 0xF);
        op2 = ROR(imm, ror * 2, s);
    }

    u32 new_cpsr = CPSR.d;

    u32 operation = SUBVAL(opcode->d, 21, 0xF);
    u32 rn        = SUBVAL(opcode->d, 16, 0xF);
    u32 rd        = SUBVAL(opcode->d, 12, 0xF);

    if (operation >= 5 && operation <= 7) {CPSR.d = (CPSR.d & ~FLAG_C) | (old_cpsr & FLAG_C);}

    switch (operation)
    {
    case 0x0: RX_xxx[rd]->d = AND(RX_xxx[rn]->d, op2,           s); break;
    case 0x1: RX_xxx[rd]->d = EOR(RX_xxx[rn]->d, op2,           s); break;
    case 0x2: RX_xxx[rd]->d = SUB(RX_xxx[rn]->d, op2,           s); break;
    case 0x3: RX_xxx[rd]->d = SUB(op2,           RX_xxx[rn]->d, s); break;
    case 0x4: RX_xxx[rd]->d = ADD(RX_xxx[rn]->d, op2,           s); break;
    case 0x5: RX_xxx[rd]->d = ADC(RX_xxx[rn]->d, op2,           s); break;
    case 0x6: RX_xxx[rd]->d = SBC(RX_xxx[rn]->d, op2,           s); break;
    case 0x7: RX_xxx[rd]->d = SBC(op2,           RX_xxx[rn]->d, s); break;
    case 0x8: (void)          AND(RX_xxx[rn]->d, op2,           s); break;
    case 0x9: (void)          EOR(RX_xxx[rn]->d, op2,           s); break;
    case 0xA: (void)          SUB(RX_xxx[rn]->d, op2,           s); break;
    case 0xB: (void)          ADD(RX_xxx[rn]->d, op2,           s); break;
    case 0xC: RX_xxx[rd]->d = ORR(RX_xxx[rn]->d, op2,           s); break;
    case 0xD: RX_xxx[rd]->d = MOV(               op2,           s); break;
    case 0xE: RX_xxx[rd]->d = BIC(RX_xxx[rn]->d, op2,           s); break;
    default:  RX_xxx[rd]->d = MVN(               op2,           s);
    }

    if (write[operation] && rd == REGISTER_PC) {NSI += WritePC(s);} else if (usePC12) {RX_xxx[REGISTER_PC]->d -= instructionlength;}
    return S_cycle + NSI;
}
//-------------------------------------------------------------------------------------------------
// Formato 6: PSR Transfer (MRS, MSR) -------------------------------------------------------------
s32 ARM_Format6()
{
    bool useSPSR = BITTEST(opcode->d, 22);

    if (!BITTEST(opcode->d, 21))
    {
        u32 rd = SUBVAL(opcode->d, 12, 0xF);
        RX_xxx[rd]->d = useSPSR ? SPSR_xxx->d : CPSR.d;
    }
    else
    {
        bool privileged = (CPSR.d & MODE_BITS) != MODE_USER;
        t32 op2;
    
        if (!BITTEST(opcode->d, 25))
        {
            u32 rm = SUBVAL(opcode->d, 0, 0xF);
            op2.d = RX_xxx[rm]->d;
        }
        else
        {
            u32 imm = SUBVAL(opcode->d, 0, 0xFF);
            u32 ror = SUBVAL(opcode->d, 8, 0xF);
            op2.d = ROR(imm, ror * 2, false);
        }        

        if (BITTEST(opcode->d, 16)) {if (useSPSR) {SPSR_xxx->w.w0.b.b0.b = op2.w.w0.b.b0.b;} else if (privileged) {CPSR.w.w0.b.b0.b = op2.w.w0.b.b0.b;}}
        if (BITTEST(opcode->d, 17)) {if (useSPSR) {SPSR_xxx->w.w0.b.b1.b = op2.w.w0.b.b1.b;} else if (privileged) {CPSR.w.w0.b.b1.b = op2.w.w0.b.b1.b;}}
        if (BITTEST(opcode->d, 18)) {if (useSPSR) {SPSR_xxx->w.w1.b.b0.b = op2.w.w1.b.b0.b;} else if (privileged) {CPSR.w.w1.b.b0.b = op2.w.w1.b.b0.b;}}
        if (BITTEST(opcode->d, 19)) {if (useSPSR) {SPSR_xxx->w.w1.b.b1.b = op2.w.w1.b.b1.b;} else                 {CPSR.w.w1.b.b1.b = op2.w.w1.b.b1.b;}}

        if (!useSPSR) {WriteCPSR();}
    }

    return S_cycle;
}
//-------------------------------------------------------------------------------------------------
// Formato 7: Multiply and Multiply-Accumulate (MUL, MLA) -----------------------------------------
s32 ARM_Format7()
{
    u32  rm         = SUBVAL(opcode->d,  0, 0xF);
    u32  rs         = SUBVAL(opcode->d,  8, 0xF);
    u32  rn         = SUBVAL(opcode->d, 12, 0xF);
    u32  rd         = SUBVAL(opcode->d, 16, 0xF);
    bool s          = BITTEST(opcode->d, 20);
    bool accumulate = BITTEST(opcode->d, 21);

    RX_xxx[rd]->d = !accumulate ? MUL(RX_xxx[rm]->d, RX_xxx[rs]->d,                s) :
                                  MLA(RX_xxx[rm]->d, RX_xxx[rs]->d, RX_xxx[rn]->d, s);
    
    return S_cycle + MultiplierArrayCycles(RX_xxx[rs]->d, true) + (!accumulate ? 0 : 1);
}
//-------------------------------------------------------------------------------------------------
// Formato 8: Multiply Long and Multiply-Accumulate Long (MULL, MLAL) -----------------------------
s32 ARM_Format8()
{
    u32  rm         = SUBVAL(opcode->d,  0, 0xF);
    u32  rs         = SUBVAL(opcode->d,  8, 0xF);
    u32  rn         = SUBVAL(opcode->d, 12, 0xF);
    u32  rd         = SUBVAL(opcode->d, 16, 0xF);
    bool s          = BITTEST(opcode->d, 20);
    bool accumulate = BITTEST(opcode->d, 21);
    bool sign       = BITTEST(opcode->d, 22);
    t64  a;

    a.d.d0.d = RX_xxx[rn]->d;
    a.d.d1.d = RX_xxx[rd]->d;

    a.q = !accumulate ? (sign ? SMULL(RX_xxx[rm]->d, RX_xxx[rs]->d,      s) : UMULL(RX_xxx[rm]->d, RX_xxx[rs]->d,      s)) :
                        (sign ? SMLAL(RX_xxx[rm]->d, RX_xxx[rs]->d, a.q, s) : UMLAL(RX_xxx[rm]->d, RX_xxx[rs]->d, a.q, s));

    RX_xxx[rn]->d = a.d.d0.d;
    RX_xxx[rd]->d = a.d.d1.d;

    return S_cycle + MultiplierArrayCycles(RX_xxx[rs]->d, sign) + (!accumulate ? 0 : 1) + 1;
}
//-------------------------------------------------------------------------------------------------
// Formato 9: Single Data Transfer (LDR, STR) -----------------------------------------------------
s32 ARM_Format9()
{
    u32 offset;

    if (BITTEST(opcode->d, 25))
    {
        u32 rm = SUBVAL(opcode->d, 0, 0xF);
        u32 op = SUBVAL(opcode->d, 5, 0x3);
        u32 sh = SUBVAL(opcode->d, 7, 0x1F);        
        offset = ShiftGroup(op, RX_xxx[rm]->d, sh, false, true);
    }
    else
    {
        offset = SUBVAL(opcode->d, 0, 0xFFF);
    }

    u32  rd   = SUBVAL(opcode->d, 12, 0xF);
    u32  rn   = SUBVAL(opcode->d, 16, 0xF);
    bool load = BITTEST(opcode->d, 20);
    bool w    = BITTEST(opcode->d, 21);
    bool u    = BITTEST(opcode->d, 23);
    bool p    = BITTEST(opcode->d, 24);
    
    gbaMemory::DataType width = BITTEST(opcode->d, 22) ? gbaMemory::TYPE_BYTE : gbaMemory::TYPE_WORD;

    return SingleDataTransfer(load, rn, offset, rd, width, true, false, p, u, w);
}
//-------------------------------------------------------------------------------------------------
// Formato 10: Halfword and Signed Data Transfer (LDRH/STRH/LDRSB/LDRSH) --------------------------
s32 ARM_Format10()
{
    u32 offset;

    if (!BITTEST(opcode->d, 22))
    {
        u32 rm = SUBVAL(opcode->d, 0, 0xF);
        offset = RX_xxx[rm]->d;
    }
    else
    {
        u32 lo = SUBVAL(opcode->d, 0, 0xF);
        u32 hi = SUBVAL(opcode->d, 8, 0xF);
        offset = lo | (hi << 4);
    }

    bool sx   = BITTEST(opcode->d, 6);    
    u32  rd   = SUBVAL(opcode->d, 12, 0xF);
    u32  rn   = SUBVAL(opcode->d, 16, 0xF);
    bool load = BITTEST(opcode->d, 20);    
    bool w    = BITTEST(opcode->d, 21);
    bool u    = BITTEST(opcode->d, 23);
    bool p    = BITTEST(opcode->d, 24);

    gbaMemory::DataType width = BITTEST(opcode->d, 5) ? gbaMemory::TYPE_HALFWORD : gbaMemory::TYPE_BYTE;

    return SingleDataTransfer(load, rn, offset, rd, width, true, sx, p, u, w);
}
//-------------------------------------------------------------------------------------------------
// Formato 11: Block Data Transfer (LDM, STM) -----------------------------------------------------
s32 ARM_Format11()
{
    u32  rlist = SUBVAL(opcode->d,  0, 0xFFFF);
    u32  rn    = SUBVAL(opcode->d, 16, 0xF);
    bool load  = BITTEST(opcode->d, 20);
    bool w     = BITTEST(opcode->d, 21);
    bool s     = BITTEST(opcode->d, 22);
    bool u     = BITTEST(opcode->d, 23);
    bool p     = BITTEST(opcode->d, 24);

    return MultipleDataTransfer(load, rlist, rn, p, u, w, s);
}
//-------------------------------------------------------------------------------------------------
// Formato 12: Single Data Swap (SWP) -------------------------------------------------------------
s32 ARM_Format12()
{
    u32 rm = SUBVAL(opcode->d,  0, 0xF);
    u32 rd = SUBVAL(opcode->d, 12, 0xF);
    u32 rn = SUBVAL(opcode->d, 16, 0xF);
    s32 N[2];
    s32 S[2];
    t32 data;

    gbaMemory::DataType width = BITTEST(opcode->d, 22) ? gbaMemory::TYPE_BYTE : gbaMemory::TYPE_WORD;

    LDR(RX_xxx[rn]->d, &data,      width, &N[0], &S[0], true, false);
    STR(RX_xxx[rn]->d, RX_xxx[rm], width, &N[1], &S[1]);

    RX_xxx[rd]->d = data.d;

    return S_cycle + N[0] + N[1] + 1;
}
//-------------------------------------------------------------------------------------------------
// Formato 13: Software Interrupt (SWI) -----------------------------------------------------------
s32 ARM_Format13()
{
    return S_cycle + EnterException(EXCEPTION_SOFTWAREINTERRUPT);
}
//-------------------------------------------------------------------------------------------------
// Formato 14: Coprocessor Data Operations (CDP) --------------------------------------------------
s32 ARM_Format14()
{
    return S_cycle + EnterException(EXCEPTION_UNDEFINEDINSTRUCTION);
}
//-------------------------------------------------------------------------------------------------
// Formato 15: Coprocessor Data Transfers (LDC, STC) ----------------------------------------------
s32 ARM_Format15()
{
    return S_cycle + EnterException(EXCEPTION_UNDEFINEDINSTRUCTION);
}
//-------------------------------------------------------------------------------------------------
// Formato 16: Coprocessor Register Transfers (MRC, MCR) ------------------------------------------
s32 ARM_Format16()
{
    u32 cp = SUBVAL(opcode->d,  8, 0xF);
    u32 rd = SUBVAL(opcode->d, 12, 0xF);
    s32 NSI;

    if (cp == COPROCESSOR_14)
    {
        if (!BITTEST(opcode->d, 20))
        {
            NSI = 2;
        }
        else
        {
            u32 data = GetPrefetch();
            if (rd == REGISTER_PC) {CPSR.d = (CPSR.d & 0xFFFFFFF) | (data & ~0xFFFFFFF);} else {RX_xxx[rd]->d = data;}
            NSI = 3;            
        }
    }
    else
    {
        NSI = EnterException(EXCEPTION_UNDEFINEDINSTRUCTION);
    }

    return S_cycle + NSI;
}
//-------------------------------------------------------------------------------------------------
// Formato 17: Undefined Instruction --------------------------------------------------------------
s32 ARM_Format17()
{
    return S_cycle + EnterException(EXCEPTION_UNDEFINEDINSTRUCTION);
}
//-------------------------------------------------------------------------------------------------
// Decodificar y ejecutar instruccion ARM ---------------------------------------------------------
s32 ARM_DecodeAndExecute()
{
    u32 cc = SUBVAL(opcode->d, 28, 0xF);

    if (!TestCondition(cc)) {return S_cycle;}

    switch (SUBVAL(opcode->d, 25, 7))
    {
    case 0:
        if ((opcode->d & 0xFFFFFF0) == 0x12FFF10) {return ARM_Format3();}

        switch (SUBVAL(opcode->d, 4, 0xF))
        {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0xA:        
        case 0xC:        
        case 0xE:
            if (BITTEST(opcode->d, 24) && !BITTEST(opcode->d, 23) && !BITTEST(opcode->d, 20))
            {
                return ((opcode->d & 0xFF0) == 0x000) ? ARM_Format6() : ARM_Format17();
            }
            else
            {
                return ARM_Format5();
            }
        case 0x9:
            if (BITTEST(opcode->d, 24))
            {
                return ((opcode->d & 0xB00F00) == 0x000000) ? ARM_Format12() : ARM_Format17();
            }
            else
            {
                if (BITTEST(opcode->d, 23))
                {
                    return ARM_Format8();
                }
                else
                {
                    return BITTEST(opcode->d, 22) ? ARM_Format17() : ARM_Format7();
                }
            }
        case 0xB:
        case 0xD:
        default:
            if (BITTEST(opcode->d, 22))
            {
                return ARM_Format10();
            }
            else
            {
                return ((opcode->d & 0xF00) == 0x000) ? ARM_Format10() : ARM_Format17();
            }
        }
    case 1:
        if (BITTEST(opcode->d, 24) && !BITTEST(opcode->d, 23) && !BITTEST(opcode->d, 20))
        {
            return BITTEST(opcode->d, 21) ? ARM_Format6() : ARM_Format17();
        }
        else
        {
            return ARM_Format5();
        }
    case 2: return ARM_Format9();
    case 3: return BITTEST(opcode->d, 4) ? ARM_Format17() : ARM_Format9();
    case 4: return ARM_Format11();
    case 5: return ARM_Format4();
    case 6: return ARM_Format15();
    default:
        if (BITTEST(opcode->d, 24))
        {
            return ARM_Format13();
        }
        else
        {
            return BITTEST(opcode->d, 4) ? ARM_Format16() : ARM_Format14();
        }
    }
}
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Set de instrucciones THUMB (16 bits)
//-------------------------------------------------------------------------------------------------
// Formato 1: move shifted register ---------------------------------------------------------------
s32 THUMB_Format1() 
{
    u32 rd     = SUBVAL(opcode->d,  0, 0x7);
    u32 rs     = SUBVAL(opcode->d,  3, 0x7);
    u32 offset = SUBVAL(opcode->d,  6, 0x1F);
    u32 op     = SUBVAL(opcode->d, 11, 0x3);

    RX_xxx[rd]->d = ShiftGroup(op, RX_xxx[rs]->d, offset, true, true);
    
    return S_cycle;
}
//-------------------------------------------------------------------------------------------------
// Formato 2: add/subtract ------------------------------------------------------------------------
s32 THUMB_Format2() 
{
    u32 rd      = SUBVAL(opcode->d, 0, 0x7);        
    u32 rs      = SUBVAL(opcode->d, 3, 0x7);
    u32 rn      = SUBVAL(opcode->d, 6, 0x7);
    u32 operand = BITTEST(opcode->d, 10) ? rn : RX_xxx[rn]->d;

    RX_xxx[rd]->d = BITTEST(opcode->d, 9) ? SUB(RX_xxx[rs]->d, operand, true) :
                                            ADD(RX_xxx[rs]->d, operand, true);

    return S_cycle;
}
//-------------------------------------------------------------------------------------------------
// Formato 3: move/compare/add/subtract immediate -------------------------------------------------
s32 THUMB_Format3()
{
    u32 imm = SUBVAL(opcode->d, 0, 0xFF);
    u32 rd  = SUBVAL(opcode->d, 8, 0x7);

    switch (SUBVAL(opcode->d, 11, 3))
    {
    case 0:  RX_xxx[rd]->d = MOV(               imm, true); break;
    case 1:  (void)          SUB(RX_xxx[rd]->d, imm, true); break;
    case 2:  RX_xxx[rd]->d = ADD(RX_xxx[rd]->d, imm, true); break;
    default: RX_xxx[rd]->d = SUB(RX_xxx[rd]->d, imm, true);
    }

    return S_cycle;
}
//-------------------------------------------------------------------------------------------------
// Formato 4: ALU operations ----------------------------------------------------------------------
s32 THUMB_Format4()
{
    u32 rd = SUBVAL(opcode->d, 0, 0x7);
    u32 rs = SUBVAL(opcode->d, 3, 0x7);
    s32 I;

    switch (SUBVAL(opcode->d, 6, 0xF))
    {
    case 0x0: RX_xxx[rd]->d = AND(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0x1: RX_xxx[rd]->d = EOR(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0x2: RX_xxx[rd]->d = LSL(RX_xxx[rd]->d, RX_xxx[rs]->d & 0xFF, true); I = 1;                                          break;
    case 0x3: RX_xxx[rd]->d = LSR(RX_xxx[rd]->d, RX_xxx[rs]->d & 0xFF, true); I = 1;                                          break;
    case 0x4: RX_xxx[rd]->d = ASR(RX_xxx[rd]->d, RX_xxx[rs]->d & 0xFF, true); I = 1;                                          break;
    case 0x5: RX_xxx[rd]->d = ADC(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0x6: RX_xxx[rd]->d = SBC(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0x7: RX_xxx[rd]->d = ROR(RX_xxx[rd]->d, RX_xxx[rs]->d & 0xFF, true); I = 1;                                          break;
    case 0x8: (void)          AND(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0x9: RX_xxx[rd]->d = SUB(0,             RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0xA: (void)          SUB(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0xB: (void)          ADD(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0xC: RX_xxx[rd]->d = ORR(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    case 0xD: RX_xxx[rd]->d = MUL(RX_xxx[rs]->d, RX_xxx[rd]->d,        true); I = MultiplierArrayCycles(RX_xxx[rd]->d, true); break;
    case 0xE: RX_xxx[rd]->d = BIC(RX_xxx[rd]->d, RX_xxx[rs]->d,        true); I = 0;                                          break;
    default:  RX_xxx[rd]->d = MVN(               RX_xxx[rs]->d,        true); I = 0;
    }

    return S_cycle + I;
}
//-------------------------------------------------------------------------------------------------
// Formato 5: Hi register operations/branch exchange ----------------------------------------------
s32 THUMB_Format5()
{
    u32 rd = SUBVAL(opcode->d, 0, 0x7) | (BITTEST(opcode->d, 7) ? 0x8 : 0);
    u32 rs = SUBVAL(opcode->d, 3, 0xF);
    s32 NS = 0;

    switch (SUBVAL(opcode->d, 8, 3))
    {
    case 0:  RX_xxx[rd]->d = ADD(RX_xxx[rd]->d, RX_xxx[rs]->d, false); if (rd == REGISTER_PC) {NS += WritePC(false);} break;
    case 1:  (void)          SUB(RX_xxx[rd]->d, RX_xxx[rs]->d, true);                                                 break;
    case 2:  RX_xxx[rd]->d = MOV(               RX_xxx[rs]->d, false); if (rd == REGISTER_PC) {NS += WritePC(false);} break;
    default: NS += BranchAbsolute(RX_xxx[rs]->d, true, false, 0);
    }

    return S_cycle + NS;
}
//-------------------------------------------------------------------------------------------------
// Formato 6: PC-relative load --------------------------------------------------------------------
s32 THUMB_Format6()
{
    u32 offset = SUBVAL(opcode->d, 0, 0xFF);
    u32 rd     = SUBVAL(opcode->d, 8, 0x7);

    gbaMemory::DataType width = gbaMemory::TYPE_WORD;
    u32 address = (RX_xxx[REGISTER_PC]->d & ~BIT(1)) + (offset * width);

    return SingleDataTransfer(true, address, rd, width, false, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 7: load/store with register offset -----------------------------------------------------
s32 THUMB_Format7()
{
    u32  rd      = SUBVAL(opcode->d, 0, 0x7);
    u32  rb      = SUBVAL(opcode->d, 3, 0x7);
    u32  ro      = SUBVAL(opcode->d, 6, 0x7);
    bool load    = BITTEST(opcode->d, 11);
    u32  address = RX_xxx[rb]->d + RX_xxx[ro]->d;

    gbaMemory::DataType width = BITTEST(opcode->d, 10) ? gbaMemory::TYPE_BYTE : gbaMemory::TYPE_WORD;

    return SingleDataTransfer(load, address, rd, width, true, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 8: load/store sign-extended byte/halfword ----------------------------------------------
s32 THUMB_Format8()
{
    u32  rd      = SUBVAL(opcode->d, 0, 0x7);
    u32  rb      = SUBVAL(opcode->d, 3, 0x7);
    u32  ro      = SUBVAL(opcode->d, 6, 0x7);
    u32  address = RX_xxx[rb]->d + RX_xxx[ro]->d;
    bool load;
    gbaMemory::DataType width;
    bool sx;

    switch (SUBVAL(opcode->d, 10, 3))
    {
    case 0:  load = false; width = gbaMemory::TYPE_HALFWORD; sx = false; break;
    case 1:  load = true;  width = gbaMemory::TYPE_BYTE;     sx = true;  break;
    case 2:  load = true;  width = gbaMemory::TYPE_HALFWORD; sx = false; break;
    default: load = true;  width = gbaMemory::TYPE_HALFWORD; sx = true;
    }

    return SingleDataTransfer(load, address, rd, width, true, sx);
}
//-------------------------------------------------------------------------------------------------
// Formato 9: load/store with immediate offset ----------------------------------------------------
s32 THUMB_Format9()
{
    u32  rd     = SUBVAL(opcode->d, 0, 0x7);
    u32  rb     = SUBVAL(opcode->d, 3, 0x7);
    u32  offset = SUBVAL(opcode->d, 6, 0x1F);
    bool load   = BITTEST(opcode->d, 11);

    gbaMemory::DataType width = BITTEST(opcode->d, 12) ? gbaMemory::TYPE_BYTE: gbaMemory::TYPE_WORD;
    u32 address = RX_xxx[rb]->d + (offset * width);

    return SingleDataTransfer(load, address, rd, width, true, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 10: load/store halfword ----------------------------------------------------------------
s32 THUMB_Format10()
{
    u32  rd      = SUBVAL(opcode->d, 0, 0x7);
    u32  rb      = SUBVAL(opcode->d, 3, 0x7);
    u32  offset  = SUBVAL(opcode->d, 6, 0x1F);
    bool load    = BITTEST(opcode->d, 11);

    gbaMemory::DataType width = gbaMemory::TYPE_HALFWORD;
    u32 address = RX_xxx[rb]->d + (offset * width);    
    
    return SingleDataTransfer(load, address, rd, width, true, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 11: SP-relative load/store -------------------------------------------------------------
s32 THUMB_Format11()
{
    u32  offset = SUBVAL(opcode->d, 0, 0xFF);
    u32  rd     = SUBVAL(opcode->d, 8, 0x7);    
    bool load   = BITTEST(opcode->d, 11);

    gbaMemory::DataType width = gbaMemory::TYPE_WORD;
    u32 address = RX_xxx[REGISTER_SP]->d + (offset * width);

    return SingleDataTransfer(load, address, rd, width, true, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 12: load address -----------------------------------------------------------------------
s32 THUMB_Format12()
{
    u32 offset = SUBVAL(opcode->d, 0, 0xFF);
    u32 rd     = SUBVAL(opcode->d, 8, 0x7);

    RX_xxx[rd]->d = (BITTEST(opcode->d, 11) ? RX_xxx[REGISTER_SP]->d : (RX_xxx[REGISTER_PC]->d & ~BIT(1))) + (offset * gbaMemory::TYPE_WORD);

    return S_cycle;
}
//-------------------------------------------------------------------------------------------------
// Formato 13: add offset to Stack Pointer --------------------------------------------------------
s32 THUMB_Format13()
{
    u32 offset    = SUBVAL(opcode->d, 0, 0x7F);
    u32 magnitude = offset * gbaMemory::TYPE_WORD;
    u32 delta     = BITTEST(opcode->d, 7) ? NEGATE(magnitude) : magnitude;

    RX_xxx[REGISTER_SP]->d += delta;

    return S_cycle;
}
//-------------------------------------------------------------------------------------------------
// Formato 14: push/pop registers -----------------------------------------------------------------
s32 THUMB_Format14()
{
    u32  rlist = SUBVAL(opcode->d, 0, 0xFF);
    bool pclr  = BITTEST(opcode->d, 8);
    bool load  = BITTEST(opcode->d, 11);
    bool p;
    bool u;

    if (load)
    {
        u = true;
        p = false;
        if (pclr) {rlist |= BIT(REGISTER_PC);}
    }
    else
    {
        u = false;
        p = true;
        if (pclr) {rlist |= BIT(REGISTER_LR);}
    }

    return MultipleDataTransfer(load, rlist, REGISTER_SP, p, u, true, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 15: multiple load/store ----------------------------------------------------------------
s32 THUMB_Format15()
{
    u32  rlist = SUBVAL(opcode->d, 0, 0xFF);
    u32  rn    = SUBVAL(opcode->d, 8, 0x7);
    bool load  = BITTEST(opcode->d, 11);

    return MultipleDataTransfer(load, rlist, rn, false, true, true, false);
}
//-------------------------------------------------------------------------------------------------
// Formato 16: conditional branch -----------------------------------------------------------------
s32 THUMB_Format16()
{
    u32 offset = SUBVAL(opcode->d, 0, 0xFF);
    u32 cc     = SUBVAL(opcode->d, 8, 0xF);
    u32 target = RX_xxx[REGISTER_PC]->d + (SIGNEX(offset, 7) << 1);

    return S_cycle + (TestCondition(cc) ? BranchAbsolute(target, false, false, 0) : 0);
}
//-------------------------------------------------------------------------------------------------
// Formato 17: software interrupt -----------------------------------------------------------------
s32 THUMB_Format17()
{
    return S_cycle + EnterException(EXCEPTION_SOFTWAREINTERRUPT);
}
//-------------------------------------------------------------------------------------------------
// Formato 18: unconditional branch ---------------------------------------------------------------
s32 THUMB_Format18()
{
    u32 offset = SUBVAL(opcode->d, 0, 0x7FF);
    u32 target = RX_xxx[REGISTER_PC]->d + (SIGNEX(offset, 10) << 1);

    return S_cycle + BranchAbsolute(target, false, false, 0);
}
//-------------------------------------------------------------------------------------------------
// Formato 19: long branch with link --------------------------------------------------------------
s32 THUMB_Format19()
{
    u32 offset = SUBVAL(opcode->d, 0, 0x7FF);
    s32 NS;

    if (!BITTEST(opcode->d, 11))
    {
        RX_xxx[REGISTER_LR]->d = RX_xxx[REGISTER_PC]->d + (SIGNEX(offset, 10) << 12);
        NS = S_cycle;
    }
    else
    {
        u32 target = RX_xxx[REGISTER_LR]->d + (offset << 1);
        u32 link   = GetNextPC() | BIT(0);
        NS = S_cycle + BranchAbsolute(target, false, true, link);
    }

    return NS;
}
//-------------------------------------------------------------------------------------------------
// Formato U: undefined instruction (ARM9) --------------------------------------------------------
s32 THUMB_FormatU()
{
    return S_cycle + EnterException(EXCEPTION_UNDEFINEDINSTRUCTION);
}
//-------------------------------------------------------------------------------------------------
// Decodificar y ejecutar instruction THUMB -------------------------------------------------------
s32 THUMB_DecodeAndExecute()
{
    switch (SUBVAL(opcode->d, 13, 7))
    {
    case 0: return (SUBVAL(opcode->d, 11, 3) == 3) ? THUMB_Format2() : THUMB_Format1();
    case 1: return THUMB_Format3();
    case 2:
        if (BITTEST(opcode->d, 12))
        {
            return BITTEST(opcode->d, 9) ? THUMB_Format8() : THUMB_Format7();
        }
        else
        {
            if (BITTEST(opcode->d, 11))
            {
                return THUMB_Format6();
            }
            else
            {
                return BITTEST(opcode->d, 10) ? THUMB_Format5() : THUMB_Format4();
            }
        }
    case 3: return THUMB_Format9();
    case 4: return BITTEST(opcode->d, 12) ? THUMB_Format11() : THUMB_Format10();
    case 5:
        if (BITTEST(opcode->d, 12))
        {
            if (BITTEST(opcode->d, 10))
            {
                return BITTEST(opcode->d, 9) ? THUMB_FormatU() : THUMB_Format14();
            }
            else
            {
                return ((opcode->d & 0xF00) == 0x000) ? THUMB_Format13() : THUMB_FormatU();
            }
        }
        else
        {
            return THUMB_Format12();
        }
    case 6:
        if (BITTEST(opcode->d, 12))
        {
            u32 icc = SUBVAL(opcode->d, 8, 0xF);
            return (icc == 0xE) ? THUMB_FormatU() : ((icc == 0xF) ? THUMB_Format17() : THUMB_Format16());
        }
        else
        {
            return THUMB_Format15();
        }
    default:
        if (BITTEST(opcode->d, 12))
        {
            exceptionlock = !BITTEST(opcode->d, 11);
            return THUMB_Format19();
        }
        else
        {
            return BITTEST(opcode->d, 11) ? THUMB_FormatU() : THUMB_Format18();
        }
    }
}
//-------------------------------------------------------------------------------------------------
}

//*************************************************************************************************