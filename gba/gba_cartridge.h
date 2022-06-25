//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "../types.h"
#include "gba_memory.h"

namespace gbaCartridge {
enum BackupType {
    BACKUP_EEPROM,
    BACKUP_SRAM,
    BACKUP_FLASH512,
    BACKUP_FLASH1M,
    BACKUP_NOID,
    BACKUP_NONE,
};

void Release();
void StoreBackup();
bool IsLoaded();
void WriteSRAMRegion(u32 address, u8 data);
u8 ReadSRAMRegion(u32 address);
bool Load(char const *filename, BackupType type, bool usertc);
void WriteROMRegion(u32 address, t32 const *data, gbaMemory::DataType width);
void ReadROMRegion(u32 address, t32 *data, gbaMemory::DataType width);
}
//*************************************************************************************************