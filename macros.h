//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include "types.h"
#include "gba\gba_memory.h"

#define T32BYTES(data) {(data)->w.w0.b.b0.b, \
                        (data)->w.w0.b.b1.b, \
                        (data)->w.w1.b.b0.b, \
                        (data)->w.w1.b.b1.b}

#define T32POINTERS(data) {&(data)->w.w0.b.b0.b, \
                           &(data)->w.w0.b.b1.b, \
                           &(data)->w.w1.b.b0.b, \
                           &(data)->w.w1.b.b1.b}

#define UNPACK_IO_BYTES(data)    u8  _m_bytes[] = T32BYTES(data);
#define UNPACK_IO_POINTERS(data) u8 *_m_bytes[] = T32POINTERS(data);

#define BEGIN_IO_TABLE(base, width) \
    for (u32 _m_io = 0; _m_io < (u32)(width); _m_io++) { \
        switch ((base) | _m_io) {

#define IO_WRITE_DIRECT(port, variable)   case port: variable = _m_bytes[_m_io];    break;
#define IO_WRITE_CALLBACK(port, callback) case port: callback(_m_bytes[_m_io]);     break;
#define IO_READ_DIRECT(port, variable)    case port: *_m_bytes[_m_io] = variable;   break;
#define IO_READ_CALLBACK(port, callback)  case port: *_m_bytes[_m_io] = callback(); break;

#define END_IO_TABLE() \
        default: (void)0;\
        }\
    }

#define READ(memory, base, data, width) \
    switch (width) {\
    case gbaMemory::TYPE_WORD:     data->w.w1.b.b1.b = memory[base | 3]; \
                                   data->w.w1.b.b0.b = memory[base | 2]; \
    case gbaMemory::TYPE_HALFWORD: data->w.w0.b.b1.b = memory[base | 1]; \
    case gbaMemory::TYPE_BYTE:     data->w.w0.b.b0.b = memory[base | 0]; break;\
    default: __assume(0); \
    }

#define WRITE(memory, base, data, width) \
    switch (width) {\
    case gbaMemory::TYPE_WORD:     memory[base | 3] = data->w.w1.b.b1.b; \
                                   memory[base | 2] = data->w.w1.b.b0.b; \
    case gbaMemory::TYPE_HALFWORD: memory[base | 1] = data->w.w0.b.b1.b; \
    case gbaMemory::TYPE_BYTE:     memory[base | 0] = data->w.w0.b.b0.b; break;\
    default: __assume(0);\
    }