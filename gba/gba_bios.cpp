//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include <fstream>
#include "../emulator.h"
#include "gba_bios.h"

namespace gbaBIOS {
const u32 m_biossize = 0x4000;

u8   m_BIOS[m_biossize];
bool m_ready = false;

bool Load(char const *filename) {
    Emulator::LogMessage("Cargando BIOS");
    if (filename == 0) {
        Emulator::LogMessage("Error nombre de archivo no especificado");
        return false;
    }
    Emulator::LogMessage("Abriendo archivo: %s", filename);
    std::ifstream biosfile(filename, std::ios::binary);
    if (!biosfile) {
        Emulator::LogMessage("Error al abrir el archivo");
        return false;
    }
    m_ready = false;
    biosfile.read((char *)m_BIOS, sizeof(m_BIOS));
    if (!biosfile) {
        Emulator::LogMessage("Error al leer el archivo (%d / %d bytes)", (u32)biosfile.gcount(), sizeof(m_BIOS));
        return false;
    }
    m_ready = true;
    t32 data;
    u32 cs = 0;
    for (u32 i = 0; i < sizeof(m_BIOS); i += 4) {
        Read(i, &data, gbaMemory::TYPE_WORD);
        cs += data.d;
    }
    bool isgba = cs == 0xBAAE187F;
    bool isnds = cs == 0xBAAE1880;
    Emulator::LogMessage("Checksum: %s (0x%X) [%s]", isgba || isnds ? "OK" : "Incorrecto", cs, isgba ? "GBA" : (isnds ? "NDS" : "Desconocido"));
    Emulator::LogMessage("BIOS cargado en el emulador\n");
    return true;
}

bool IsLoaded() {
    return m_ready;
}

void Read(u32 address, t32 *data, gbaMemory::DataType width) {
    u32 base = ALIGN(address, width);
    if (base < m_biossize) {READ(m_BIOS, base, data, width);}
}
}
//*************************************************************************************************