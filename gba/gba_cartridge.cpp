//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#define _CRT_SECURE_NO_WARNINGS

#include <fstream>
#include <ctime>
#include <vector>
#include "../emulator.h"
#include "gba_cartridge.h"

namespace gbaCartridge {
enum FlashMode {
    MODE_NONE,
    MODE_ID,
    MODE_ERASE,
    MODE_WRITE,
    MODE_BANK
};

struct ROMHeader {
    u8 ROMentrypoint[4];
    u8 logo[156];
    u8 gametitle[12];
    u8 gamecode[4];
    u8 makercode[2];
    u8 v0x96;
    u8 unitcode;
    u8 devicetype;
    u8 reserved1[7];
    u8 softwareversion;
    u8 headerchecksum;
    u8 reserved2[2];
};

char const * const m_backupstrings[5] = {
    "EEPROM_V",
    "SRAM_V",
    "FLASH_V",
    "FLASH512_V",
    "FLASH1M_V"
};

char const * const m_backuptypestr[6] = {
    "EEPROM 512B / 8KB",
    "SRAM 32KB",
    "Flash ROM 64KB",
    "Flash ROM 128KB",
    "Sin ID",
    "Ninguno"
};

const size_t m_backupstringslength[5] = {8, 6, 7, 10, 9};
const u32    m_backuptypesizes[6]     = {8192, 32768, 65536, 131072, 0, 0};
const u16    m_flash512id             = 0x1CC2;
const u16    m_flash1Mid              = 0x09C2;

bool m_ready = false;

u8   *m_ROM;
u8   *m_backup;
u8   *m_flash;
char *m_savfilename;

u32        m_romsize;
u32        m_backupsize;
BackupType m_backuptype;

FlashMode  m_flashmode;
t16        m_flashid;
u8         m_flash0x5555;
u8         m_flash0x2AAA;

bool            m_eepromdetect;
std::vector<u8> m_eeprombits;
bool            m_eepromread;
u32             m_eeprombitsleft;
u32             m_eepromreadadr;

char m_title[13];
char m_code[9];
u32  m_headerchecksum;
char m_backupid[14];

u32 m_GPIODIR;
u32 m_GPIOCNT;

bool            m_rtcenable;
std::vector<u8> m_rtcbits;
u32             m_rtcSCK;
u32             m_rtcCS;
u32             m_rtcSIO;
bool            m_rtcincommand;
u32             m_rtcstatus;
tm              m_rtcstr;
u32             m_rtcbitsleft;
u32             m_rtcbit;

void Release() {
    if (m_ROM         != 0) {delete [] m_ROM;         m_ROM         = 0;}
    if (m_backup      != 0) {delete [] m_backup;      m_backup      = 0;}
    if (m_savfilename != 0) {delete [] m_savfilename; m_savfilename = 0;}

    m_ready = false;
}

void CorrectAscii7BitString(char *str, u32 size) {
    for (u32 i = 0; i < size; i++) {
        if ((str[i] > 0 && str[i] < 32) || str[i] >= 127) {str[i] = '?';}
    }
}

bool LoadROM(char const *filename) {
    Emulator::LogMessage("Cargando ROM");
    if (filename == 0) {
        Emulator::LogMessage("Nombre de archivo no especificado");
        return false;
    }
    Emulator::LogMessage("Abriendo archivo: %s", filename);
    std::ifstream romfile(filename, std::ios::ate | std::ios::binary);
    if (!romfile) {
        Emulator::LogMessage("Error al abrir el archivo");
        return false;
    }
    u32 romfilesize = (u32)romfile.tellg();
    Emulator::LogMessage("Tama\xC3\xB1o de archivo: %d bytes", romfilesize);
    if (romfilesize > 33554432) {
        Emulator::LogMessage("Solo se utilizaran los primeros 32 MB del archivo");
        romfilesize = 33554432;
    }
    m_romsize = (romfilesize & 3) != 0 ? (romfilesize + 4) & ~3 : romfilesize;
    if (m_romsize < 256) {m_romsize = 256;}
    m_ROM = new(std::nothrow) u8[m_romsize];
    if (m_ROM == 0) {
        Emulator::LogMessage("Error al asignar memoria para ROM (%d bytes)", m_romsize);
        return false;
    }
    memset(m_ROM, 0xFF, m_romsize);
    romfile.seekg(0, romfile.beg);
    romfile.read((char *)m_ROM, m_romsize);
    if (!romfile) {
        Emulator::LogMessage("Error al leer el archivo (%d / %d bytes)", (u32)romfile.gcount(), m_romsize);
        return false;
    }
    Emulator::LogMessage("ROM cargado en %d bytes", m_romsize);
    Emulator::LogMessage("Informacion de ROM");
    ROMHeader *rh = (ROMHeader *)m_ROM;
    memcpy(m_title, rh->gametitle, 12);
    m_title[12] = '\0';
    CorrectAscii7BitString(m_title, 12);
    Emulator::LogMessage("Titulo: %s", m_title);
    strcpy(m_code, "AGB-");
    memcpy(&m_code[4], rh->gamecode, 4);
    m_code[8] = '\0';
    CorrectAscii7BitString(m_code, 8);
    Emulator::LogMessage("Codigo: %s", m_code);
    u32 cs = 0;
    for (s32 i = 0xA0; i < 0xBD; i++) {cs -= m_ROM[i];}
    m_headerchecksum = (cs - 0x19) & 0xFF;
    Emulator::LogMessage("Checksum: %s (0x%X / 0x%X)", m_headerchecksum == rh->headerchecksum ? "OK" : "Incorrecto", m_headerchecksum, rh->headerchecksum);
    return true;
}

void GetBackupID() {
    bool backupidfound = false;
    u32  bx;
    u32  cmplen;
    u32  fullen;

    for (bx = 0; bx < m_romsize && !backupidfound; bx += 4) {
        for (u32 i = 0; i < 5; i++) {
            cmplen = m_backupstringslength[i];
            fullen = cmplen + 3;
            if ((bx + fullen) <= m_romsize && memcmp(&m_ROM[bx], m_backupstrings[i], cmplen) == 0) {
                backupidfound = true;
                switch (i) {
                case 0: m_backuptype = BACKUP_EEPROM;   break;
                case 1: m_backuptype = BACKUP_SRAM;     break;
                case 2:
                case 3: m_backuptype = BACKUP_FLASH512; break;
                case 4: m_backuptype = BACKUP_FLASH1M;  break;
                }
                memcpy(m_backupid, &m_ROM[bx], fullen);
                m_backupid[fullen] = '\0';
                CorrectAscii7BitString(m_backupid, fullen);
                break;
            }
        }
    }

    if (!backupidfound) {
        m_backuptype = BACKUP_NOID;
        strcpy(m_backupid, "-");
    }

    Emulator::LogMessage("Backup ID: %s (%s)", m_backuptypestr[m_backuptype], m_backupid);
}

bool LoadBackup(char const *filename, BackupType forcedtype) {
    Emulator::LogMessage("Cargando backup");
    if (forcedtype != BACKUP_NOID) {
        m_backuptype = forcedtype;
        Emulator::LogMessage("Usando backup: %s", m_backuptypestr[m_backuptype]);
    }
    else if (m_backuptype == BACKUP_NOID) {
        Emulator::LogMessage("Sin ID de backup");
        Emulator::LogMessage("Se asumira que el programa no utiliza backup");
        m_backuptype = BACKUP_NONE;
    }
    m_flash = m_backup = 0;
    m_backupsize = m_backuptypesizes[m_backuptype];
    if (m_backupsize == 0) {return true;}
    m_backup = new(std::nothrow) u8[m_backupsize];
    if (m_backup == 0) {
        Emulator::LogMessage("Error al asignar memoria para backup (%d bytes)", m_backupsize);
        return false;
    }
    if (m_backuptype == BACKUP_FLASH512 || m_backuptype == BACKUP_FLASH1M) {
        m_flash       = m_backup;
        m_flashid.w   = m_backuptype == BACKUP_FLASH512 ? m_flash512id : m_flash1Mid;
        m_flash0x5555 = m_flash0x2AAA = 0;
        m_flashmode   = MODE_NONE;
    }
    else if (m_backuptype == BACKUP_EEPROM) {
        m_eeprombits.clear();
        m_eeprombits.reserve(81);
        m_eepromread   = false;
        m_eepromdetect = false;
    }
    memset(m_backup, m_backuptype == BACKUP_SRAM ? 0x00 : 0xFF, m_backupsize);
    Emulator::LogMessage("Generando el nombre del archivo .sav");
    if (filename == 0) {
        Emulator::LogMessage("Error nombre de archivo no especificado");
        return false;
    }
    size_t      fnamelen    = strlen(filename);
    char const *dotpos      = strrchr(filename, '.');
    size_t      savnamebody = dotpos != 0 ? fnamelen - strlen(dotpos) : fnamelen;
    size_t      savnamelen  = savnamebody + 4;
    m_savfilename = new(std::nothrow) char[savnamelen + 1];
    if (m_savfilename == 0) {
        Emulator::LogMessage("Error al asignar memoria para el nombre del archivo .sav (%d + 1 bytes)", savnamelen);
        return false;
    }
    memcpy(m_savfilename, filename, savnamebody);
    strcpy(&m_savfilename[savnamebody], ".sav");
    Emulator::LogMessage("Abriendo archivo: %s", m_savfilename);
    std::ifstream savfile(m_savfilename, std::ios::binary);
    if (!savfile) {
        Emulator::LogMessage("Error al abrir el archivo");
        return true;
    }
    savfile.read((char *)m_backup, m_backupsize);
    Emulator::LogMessage("Backup cargado en el emulador (%d bytes)", (u32)savfile.gcount());
    return true;
}

void StoreBackup() {
    if (m_backup == 0) {
        Emulator::LogMessage("No hay datos para almacenar");
        return;
    }
    if (m_savfilename == 0) {
        Emulator::LogMessage("Error al generar el nombre del archivo .sav");
        return;
    }
    Emulator::LogMessage("Abriendo archivo: %s", m_savfilename);
    std::ofstream savfile(m_savfilename, std::ios::binary);
    if (!savfile) {
        Emulator::LogMessage("Error al abrir el archivo");
        return;
    }
    savfile.write((char *)m_backup, m_backupsize);
    Emulator::LogMessage(!savfile ? "Error al escribir al archivo" : "Backup almacenado");
}

bool IsLoaded() {return m_ready;}

void WriteSRAM(u32 address, u8 data) {m_backup[address] = data;}
u8 ReadSRAM(u32 address) {return m_backup[address];}

bool CommandFlash() {return m_flash0x5555 == 0xAA && m_flash0x2AAA == 0x55;}

void WriteFlashROM(u32 address, u8 data) {
    if (m_flashmode == MODE_WRITE) {
        m_flash[address] = data;
        m_flashmode      = MODE_NONE;
    }
    else if (address == 0x2AAA) {
        m_flash0x2AAA = data;
    }
    else if (m_flashmode == MODE_ERASE && data == 0x10 && address == 0x5555 && CommandFlash()) {
        memset(m_backup, 0xFF, m_backupsize);
        m_flash0x5555 = 0x10;
        m_flashmode   = MODE_NONE;
    }
    else if (m_flashmode == MODE_ERASE && data == 0x30 && CommandFlash()) {
        memset(&m_flash[address & 0xF000], 0xFF, 0x1000);
        m_flashmode = MODE_NONE;
    }
    else if (m_flashmode == MODE_BANK && m_backuptype == BACKUP_FLASH1M && address == 0) {
        m_flash     = &m_backup[(data & 0x01) != 0 ? 0x10000 : 0];
        m_flashmode = MODE_NONE;
    }
    else if (address == 0x5555 && CommandFlash()) {
        switch (data) {
        case 0x90: m_flashmode = MODE_ID;    break;
        case 0xF0: m_flashmode = MODE_NONE;  break;
        case 0x80: m_flashmode = MODE_ERASE; break;
        case 0xA0: m_flashmode = MODE_WRITE; break;
        case 0xB0: m_flashmode = MODE_BANK;  break;
        }
        m_flash0x5555 = data;
    }
    else if (address == 0x5555) {
        m_flash0x5555 = data;
    }
}

u8 ReadFlashROM(u32 address) {
    u8 ret;
    switch (address) {
    case 0:  ret = (m_flashmode == MODE_ID) ? m_flashid.b.b0.b : m_flash[address]; break;
    case 1:  ret = (m_flashmode == MODE_ID) ? m_flashid.b.b1.b : m_flash[address]; break;
    default: ret = m_flash[address];
    }
    return ret;
}

void WriteSRAMRegion(u32 address, u8 data) {
    switch (m_backuptype) {
    case BACKUP_SRAM:     WriteSRAM(address & 0x7FFF, data);     break;
    case BACKUP_FLASH512:
    case BACKUP_FLASH1M:  WriteFlashROM(address & 0xFFFF, data); break;
    }
}

u8 ReadSRAMRegion(u32 address) {
    u8 ret;
    switch (m_backuptype) {
    case BACKUP_SRAM:     ret = ReadSRAM(address & 0x7FFF);     break;
    case BACKUP_FLASH512:
    case BACKUP_FLASH1M:  ret = ReadFlashROM(address & 0xFFFF); break;
    default:              ret = 0x00;
    }
    return ret;
}

u8 ToBCD8(u32 byte) {return (byte % 10) | (((byte / 10) % 10) << 4);}

u32 GetEEPROMAddress(u32 buswidth) {
    u32 adr = 0;
    for (u32 i = 0; i < buswidth; i++) {adr |= m_eeprombits[2 + i] << (buswidth - 1 - i);}
    if (buswidth == 14) {adr &= 0x3FF;}
    return adr * 8;
}

void SetEEPROMWrite(u32 buswidth) {
    u32 eepromwriteadr = GetEEPROMAddress(buswidth);
    u8  byte;

    for (int i = 0; i < 8; i++) {
        byte = 0;
        for (int j = 0; j < 8; j++) {byte |= m_eeprombits[buswidth + 2 + j + (8 * i)] << (7 - j);}
        m_backup[eepromwriteadr + i] = byte;
    }
}

void SetEEPROMRead(u32 buswidth) {
    m_eepromread     = true;
    m_eepromreadadr  = GetEEPROMAddress(buswidth);
    m_eeprombitsleft = 67;    
}

void WriteEEPROM(u32 data) {
    m_eeprombits.push_back(data & 1);
    if (m_eeprombits.size() > 81) {m_eeprombits.clear();}
}

u32 ReadEEPROM() {
    u32 bit;
    u32 size;

    if (m_eepromread) {
        bit = m_eeprombitsleft > 64 ? 1 : ((m_backup[m_eepromreadadr + ((64 - m_eeprombitsleft) / 8)] >> (7 - ((64 - m_eeprombitsleft) % 8))) & 1);
        if (--m_eeprombitsleft == 0) {m_eepromread = false;}
    }
    else {
        size = m_eeprombits.size();
        switch (size) {
        case 9:  SetEEPROMRead(6);   break;
        case 17: SetEEPROMRead(14);  break;
        case 73: SetEEPROMWrite(6);  break;
        case 81: SetEEPROMWrite(14); break;
        }
        if (!m_eepromdetect) {            
            if      (size ==  9 || size == 73) {Emulator::LogMessage("Se detecto EEPROM de 512B"); m_eepromdetect = true;}
            else if (size == 17 || size == 81) {Emulator::LogMessage("Se detecto EEPROM de 8KB");  m_eepromdetect = true;}
        }
        bit = 1;
        m_eeprombits.clear();
    }

    return bit;
}

void UpdateRTCRegisters() {
    time_t t;

    t        = time(0);
    m_rtcstr = *localtime(&t);

    m_rtcstr.tm_year = m_rtcstr.tm_year - 100;
    m_rtcstr.tm_mon++;

    m_rtcstr.tm_year = ToBCD8(m_rtcstr.tm_year);
    m_rtcstr.tm_mon  = ToBCD8(m_rtcstr.tm_mon);
    m_rtcstr.tm_mday = ToBCD8(m_rtcstr.tm_mday);
    m_rtcstr.tm_wday = ToBCD8(m_rtcstr.tm_wday);
    m_rtcstr.tm_hour = ToBCD8(m_rtcstr.tm_hour);
    m_rtcstr.tm_min  = ToBCD8(m_rtcstr.tm_min);
    m_rtcstr.tm_sec  = ToBCD8(m_rtcstr.tm_sec);
}

void EnterRTCCommand(u32 bytes) {
    m_rtcbitsleft  = bytes * 8;
    m_rtcincommand = true;
}

void WriteRTC(u32 bits) {
    u32 prevSCK;
    u32 cmd;

    prevSCK  = m_rtcSCK;
    m_rtcCS  = bits & BIT(2);
    m_rtcSCK = bits & BIT(0);
    m_rtcSIO = bits & BIT(1);

    if (m_rtcCS != 0 && prevSCK != 0 && m_rtcSCK == 0) {
        if (!m_rtcincommand) {
            m_rtcbits.push_back((m_rtcSIO >> 1) & 0xFF);
            if (m_rtcbits.size() >= 8) {
                cmd = 0;
                for (int i = 0; i < 8; i++) {cmd |= m_rtcbits[i] << i;}
                m_rtcbits.clear();
                m_rtcstatus = SUBVAL(cmd, 4, 7);
                switch (m_rtcstatus) {
                case 0:                                           break; // force reset (0 bytes)
                case 1:                       EnterRTCCommand(1); break; // 0xFF        (1 byte)
                case 2: UpdateRTCRegisters(); EnterRTCCommand(7); break; // date time   (7 bytes)
                case 3:                                           break; // force irq   (0 bytes)
                case 4:                       EnterRTCCommand(1); break; // control     (1 byte)
                case 5:                       EnterRTCCommand(1); break; // 0xFF        (1 byte)
                case 6: UpdateRTCRegisters(); EnterRTCCommand(3); break; // time        (3 bytes)
                case 7:                       EnterRTCCommand(1); break; // 0xFF        (1 byte)
                }
            }
        }
        else {
            switch (m_rtcstatus) {
            case 2:
                if      (m_rtcbitsleft > 48) {m_rtcbit = (m_rtcstr.tm_year >> (8 - (m_rtcbitsleft - 48)) & 1) << 1;}
                else if (m_rtcbitsleft > 40) {m_rtcbit = (m_rtcstr.tm_mon  >> (8 - (m_rtcbitsleft - 40)) & 1) << 1;}
                else if (m_rtcbitsleft > 32) {m_rtcbit = (m_rtcstr.tm_mday >> (8 - (m_rtcbitsleft - 32)) & 1) << 1;}
                else if (m_rtcbitsleft > 24) {m_rtcbit = (m_rtcstr.tm_wday >> (8 - (m_rtcbitsleft - 24)) & 1) << 1;}
                else if (m_rtcbitsleft > 16) {m_rtcbit = (m_rtcstr.tm_hour >> (8 - (m_rtcbitsleft - 16)) & 1) << 1;}
                else if (m_rtcbitsleft >  8) {m_rtcbit = (m_rtcstr.tm_min  >> (8 - (m_rtcbitsleft -  8)) & 1) << 1;}
                else                         {m_rtcbit = (m_rtcstr.tm_sec  >> (8 - (m_rtcbitsleft     )) & 1) << 1;}
                break;
            case 4:
                switch (m_rtcbitsleft) {
                case 1: m_rtcbit = 0; break;
                case 2: m_rtcbit = 2; break;
                case 3: m_rtcbit = 0; break;
                case 4: m_rtcbit = 0; break;
                case 5: m_rtcbit = 0; break;
                case 6: m_rtcbit = 0; break;
                case 7: m_rtcbit = 0; break;
                case 8: m_rtcbit = 0; break;
                }
                break;
            case 6:
                if      (m_rtcbitsleft > 16) {m_rtcbit = (m_rtcstr.tm_hour >> (8 - (m_rtcbitsleft - 16)) & 1) << 1;}
                else if (m_rtcbitsleft >  8) {m_rtcbit = (m_rtcstr.tm_min  >> (8 - (m_rtcbitsleft - 8 )) & 1) << 1;}
                else                         {m_rtcbit = (m_rtcstr.tm_sec  >> (8 - (m_rtcbitsleft     )) & 1) << 1;}
                break;
            default: m_rtcbit = 2;
            }
            if (--m_rtcbitsleft == 0) {m_rtcincommand = false;}
        }
    }
}

void InitRTC(bool enable) {
    m_GPIODIR      = 0;
    m_GPIOCNT      = 0;
    m_rtcenable    = enable;
    m_rtcincommand = false;
    m_rtcbit       = 0;
    m_rtcbits.clear();
}

void WriteGPIO(u32 address, u32 bits) {
    switch (address) {
    case 0x080000C4: WriteRTC(bits & 7);     break;
    case 0x080000C6: m_GPIODIR = bits & 0xF; break;
    case 0x080000C8: m_GPIOCNT = bits & 0x1; break;
    }
}

void ReadGPIO(u32 address, u8 *data) {
    switch (address) {
    case 0x080000C4: *data = (m_rtcSCK | (!BITTEST(m_GPIODIR, 1) ? m_rtcbit : m_rtcSIO) | m_rtcCS) & 0xFF; break;
    case 0x080000C5: *data = 0;                                                                            break;
    case 0x080000C6: *data = m_GPIODIR & 0xFF;                                                             break;
    case 0x080000C7: *data = 0;                                                                            break;
    case 0x080000C8: *data = m_GPIOCNT & 0xFF;                                                             break;
    case 0x080000C9: *data = 0;                                                                            break;
    }
}

bool Load(char const *filename, BackupType type, bool usertc) {
    if (!LoadROM(filename)) {return false;}
    GetBackupID();
    if (!LoadBackup(filename, type)) {return false;}
    InitRTC(usertc);
    m_ready = true;    
    return m_ready;
}

void WriteROMRegion(u32 address, t32 const *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);

    if (m_backuptype == BACKUP_EEPROM && base >= (m_romsize <= 16777216 ? 0x0D000000U : 0x0DFFFF00U) && base < 0x0E000000) {
        switch (width) {
        case gbaMemory::TYPE_WORD:     WriteEEPROM(data->w.w1.w);
        case gbaMemory::TYPE_HALFWORD:
        case gbaMemory::TYPE_BYTE:     WriteEEPROM(data->w.w0.w);
        }
    }
    else if (m_rtcenable && base >= 0x080000C4 && base < 0x080000CA) {
        switch (width) {
        case gbaMemory::TYPE_WORD:     WriteGPIO(base | 2, data->w.w1.w);
        case gbaMemory::TYPE_HALFWORD:
        case gbaMemory::TYPE_BYTE:     WriteGPIO(base | 0, data->w.w0.w);
        }
    }    
}

void ReadROMRegion(u32 address, t32 *data, gbaMemory::DataType width) {
    u32 port = ALIGN(address, width);
    u32 base = port & 0x01FFFFFF;

    if (base < m_romsize) {
        switch (width) {
        case gbaMemory::TYPE_WORD:     data->w.w1.b.b1.b = m_ROM[base | 3];
                                       data->w.w1.b.b0.b = m_ROM[base | 2];
        case gbaMemory::TYPE_HALFWORD: data->w.w0.b.b1.b = m_ROM[base | 1];
        case gbaMemory::TYPE_BYTE:     data->w.w0.b.b0.b = m_ROM[base | 0];
        }
    }
    else {
        switch (width) {
        case gbaMemory::TYPE_WORD:     data->d = 0;           break;
        case gbaMemory::TYPE_HALFWORD: data->w.w0.w = 0;      break;
        case gbaMemory::TYPE_BYTE:     data->w.w0.b.b0.b = 0; break;
        }
    }

    if (m_GPIOCNT != 0 && port >= 0x080000C4 && port < 0x080000CA) {
        switch (width) {
        case gbaMemory::TYPE_WORD:     ReadGPIO(port | 3, &data->w.w1.b.b1.b);
                                       ReadGPIO(port | 2, &data->w.w1.b.b0.b);
        case gbaMemory::TYPE_HALFWORD: ReadGPIO(port | 1, &data->w.w0.b.b1.b);
        case gbaMemory::TYPE_BYTE:     ReadGPIO(port | 0, &data->w.w0.b.b0.b);
        }
    }
    else if (m_backuptype == BACKUP_EEPROM && port >= (m_romsize <= 16777216 ? 0x0D000000U : 0x0DFFFF00U) && port < 0x0E000000) {
        switch (width) {
        case gbaMemory::TYPE_WORD:     data->w.w0.w      = ReadEEPROM() & 0xFFFF;
                                       data->w.w1.w      = ReadEEPROM() & 0xFFFF; break;
        case gbaMemory::TYPE_HALFWORD: data->w.w0.w      = ReadEEPROM() & 0xFFFF; break;
        case gbaMemory::TYPE_BYTE:     data->w.w0.b.b0.b = ReadEEPROM() & 0xFF;   break;
        }
    }
}
}
//*************************************************************************************************