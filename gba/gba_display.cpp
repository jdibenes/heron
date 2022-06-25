//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include <cstring>
#include "../emulator.h"
#include "gba_control.h"
#include "gba_display.h"
#include "gba_dma.h"
#include "gba_keyinput.h"

namespace gbaDisplay
{
const s32 m_lineclk = 960;
const s32 m_hblankclk = 272;
s32 m_ticks;
s32 m_mode;

const u32  m_screenwidth  = 240;
const u32  m_screenheight = 160;

u8 m_PaletteRAM[0x400];
u8 m_VRAM[0x18000];
u8 m_OAM[0x400];
bool m_bitmapmode;
t16 m_DISPCNT;
t8 m_u0x04000002;
t16 m_DISPSTAT;
t8 m_VCOUNT;

u16 m_framebuffer[GBA_SCREENHEIGHT][GBA_SCREENWIDTH];

u32 m_bgmode;

t16 m_BGXCNT[4];
t16 m_BGXHOFS[4];
t16 m_BGXVOFS[4];
t16 m_BG2PA;
t16 m_BG2PB;
t16 m_BG2PC;
t16 m_BG2PD;
t16 m_BG2X_L;
t16 m_BG2X_H;
t16 m_BG2Y_L;
t16 m_BG2Y_H;
t16 m_BG3PA;
t16 m_BG3PB;
t16 m_BG3PC;
t16 m_BG3PD;
t16 m_BG3X_L;
t16 m_BG3X_H;
t16 m_BG3Y_L;
t16 m_BG3Y_H;
t16 m_WININ;
t16 m_WINOUT;
t16 m_MOSAIC;
t16 m_BLDCNT;

u32  GetDisplayFrame()         {return SUBVAL(m_DISPCNT.w, 4, 1);}
bool IsHblankFree()            {return BITTEST(m_DISPCNT.w,  5);}
bool IsOBJModeOneDimensional() {return BITTEST(m_DISPCNT.w,  6);}
bool IsForcedBlank()           {return BITTEST(m_DISPCNT.w,  7);}

bool IsBGEnabled(u32 bg)       {return BITTEST(m_DISPCNT.w, 8 + (bg & 3));}

bool IsOBJEnabled()            {return BITTEST(m_DISPCNT.w, 12);}
bool IsWindow0Enabled()        {return BITTEST(m_DISPCNT.w, 13);}
bool IsWindow1Enabled()        {return BITTEST(m_DISPCNT.w, 14);}
bool IsOBJWindowEnabled()      {return BITTEST(m_DISPCNT.w, 15);}
bool IsOutsideWindowEnabled()  {return IsWindow0Enabled() || IsWindow1Enabled() || IsOBJWindowEnabled();}

bool IsBGModeBitmap() {return m_bitmapmode;}

class Palette
{
private:
    static const u16 *m_bgpalette;
    static const u16 *m_objpalette;

public:
    static u16 GetBackdropColor();
    static u16 GetBGColor16x16(u32 palette, u32 color);
    static u16 GetOBJColor16x16(u32 palette, u32 color);
    static u16 GetBGColor256x1(u32 index);
    static u16 GetOBJColor256x1(u32 index);
};

const u16 *Palette::m_bgpalette  = ((u16 *)&m_PaletteRAM[0]);
const u16 *Palette::m_objpalette = ((u16 *)&m_PaletteRAM[0x200]);

u16 Palette::GetBackdropColor()                       {return m_bgpalette [0];}
u16 Palette::GetBGColor16x16(u32 palette, u32 color)  {return m_bgpalette [((palette & 0xF) * 16) + (color & 0xF)];}
u16 Palette::GetOBJColor16x16(u32 palette, u32 color) {return m_objpalette[((palette & 0xF) * 16) + (color & 0xF)];}
u16 Palette::GetBGColor256x1(u32 index)               {return m_bgpalette [index & 0xFF];}
u16 Palette::GetOBJColor256x1(u32 index)              {return m_objpalette[index & 0xFF];}

enum DotType
{
    DOT_PRIORITYBITS       = BIT(0) | BIT(1),
    DOT_LOWESTPRIORITY      = BIT(0) | BIT(1),
    DOT_HIGHESTPRIORITY    = 0,
    DOT_FROMBG             = 0,
    DOT_TRANSPARENT   = BIT(2),
    DOT_OPAQUE        = 0,
    DOT_CSENONE          = 0,
    DOT_CSEALPHABLENDOBJ = BIT(3),
    DOT_CSEALPHABLEND1ST = BIT(4),
    DOT_CSEALPHABLEND2ND = BIT(5),
    DOT_CSEMOREBRIGHT1ST = BIT(6),
    DOT_CSELESSBRIGHT1ST = BIT(7),
    DOT_CSEALPHABLENDALL = DOT_CSEALPHABLENDOBJ | DOT_CSEALPHABLEND1ST,
    DOT_CSEALL        = DOT_CSEALPHABLEND1ST | DOT_CSEMOREBRIGHT1ST | DOT_CSELESSBRIGHT1ST,
    DOT_WINDOWBITS    = BIT(8) | BIT(9),
    DOT_OUTSIDEWINDOW = 0,
    DOT_OBJWINDOW     = BIT(8),
    DOT_WINDOW1       = BIT(9),
    DOT_WINDOW0       = BIT(8) | BIT(9),
    DOT_WINDOWUSEBG0  = BIT(10),
    DOT_WINDOWUSEBG1  = BIT(11),
    DOT_WINDOWUSEBG2  = BIT(12),
    DOT_WINDOWUSEBG3  = BIT(13),
    DOT_WINDOWUSEOBJ  = BIT(14),
    DOT_WINDOWUSECSE  = BIT(15),
    DOT_WINDOWUSEALL  = DOT_WINDOWUSEBG0 | DOT_WINDOWUSEBG1 | DOT_WINDOWUSEBG2 | DOT_WINDOWUSEBG3 | DOT_WINDOWUSEOBJ | DOT_WINDOWUSECSE
};

namespace ReferencePoint
{
u32 FixedPointToInteger(u32 f) {return (s32)f >> 8;}
}

// Procesamiento de OBJs --------------------------------------------------------------------------
class OBJ
{
private:
    static const u32 m_dimension[3][4][2];
    static u8 const * const m_tilebase;

    static u16 m_line[m_screenwidth];
    static u16 m_attr[m_screenwidth];
    static u16 m_objw[m_screenwidth];
    static u32 m_hmosaic;
    static u32 m_vmosaic;
    static u16 m_objwflags;
    static u16 m_outwflags;

public:
    static void Reset();
    static void SetMosaic(u32 h, u32 v);
    static void SetOBJWindowFlags(u16 objwflags);
    static void SetOutsideWindowFlags(u16 outwflags);
    static void RenderLine();    
    static void GetLine(u16 const *&line, u16 const *&attr);
    static void GetWindowLine(u16 *&objw);
};

const u32 OBJ::m_dimension[3][4][2] =
{
// Longitud: x en 0, y en 1 (2 dimensiones)   // Forma: 0 a 2 (3 formas)
    {{ 8,  8}, {16, 16}, {32, 32}, {64, 64}}, // Cuadrado
    {{16,  8}, {32,  8}, {32, 16}, {64, 32}}, // Horizontal
    {{ 8, 16}, { 8, 32}, {16, 32}, {32, 64}}  // Vertical
// Tamaño: 0 a 3 (4 tamaños)
};

u8 const * const OBJ::m_tilebase = &m_VRAM[0x10000];

u16 OBJ::m_line[m_screenwidth];
u16 OBJ::m_attr[m_screenwidth];
u16 OBJ::m_objw[m_screenwidth];
u32 OBJ::m_hmosaic;
u32 OBJ::m_vmosaic;
u16 OBJ::m_objwflags;
u16 OBJ::m_outwflags;

void OBJ::Reset()
{
    m_hmosaic = 1;
    m_vmosaic = 1;
}

void OBJ::SetMosaic(u32 h, u32 v)
{
    m_hmosaic = h;
    m_vmosaic = v;
}

void OBJ::SetOBJWindowFlags(u16 objwflags)     {m_objwflags = objwflags;}
void OBJ::SetOutsideWindowFlags(u16 outwflags) {m_outwflags = outwflags;}

void OBJ::RenderLine()
{
    u32  objbase;
    u32  ypos;
    u32  mode;
    u32  shape;
    u32  xpos;
    u32  tilecol;
    u32  hdelta;
    u32  hlim;
    u32  vlim;
    u32  tilerow;
    u32  size;
    u32  chr;
    u32  mosaicrow;
    u32  mosaiccol;
    bool users;
    bool use2x;
    bool use256x1;
    bool usemosaic;
    bool onedimensional;
    u32  color;
    u32  palette;
    u32  htileofs;
    u32  vstart;
    u32  vend;
    u32  hstart;
    u32  hend;
    u32  dot;
    u32  pxbase;
    u32  xdelta;
    u32  ydelta;
    u32  hsize;
    u32  vsize;
    u32  hcenter;
    u32  vcenter;
    t32  PA;
    t32  PB;
    t32  PC;
    t32  PD;
    u16  priority;
    u16  extraflag;
    u16  pixel;
    t16  attribute[3];
    bool objenabled;
    bool objwindowenabled;
    u32  objhcenter;
    u32  objvcenter;
    u16  winxflags;
    u32  hmosaic;
    u32  vmosaic;

    objenabled       = IsOBJEnabled();
    objwindowenabled = IsOBJWindowEnabled();

    winxflags = IsOutsideWindowEnabled() ? m_outwflags : DOT_WINDOWUSEALL;

    for (u32 i = 0; i < m_screenwidth; ++i)
    {
        m_attr[i] = DOT_TRANSPARENT;
        m_objw[i] = winxflags;
    }

    if (!objenabled && !objwindowenabled) {return;}

    hcenter    = 0;
    vcenter    = 0;
    hdelta     = 0;
    ydelta     = 0;
    xdelta     = 0;
    PD.d       = 0;
    PC.d       = 0;
    PB.d       = 0;
    PA.d       = 0;
    objhcenter = 0;
    objvcenter = 0;

    for (u32 entry = 0; entry < 128; ++entry)
    {
        objbase = entry << 3;

        attribute[0].b.b0.b = m_OAM[objbase + 0];
        attribute[0].b.b1.b = m_OAM[objbase + 1];
        
        users = BITTEST(attribute[0].w, 8);
        use2x = BITTEST(attribute[0].w, 9);

        if (!users && use2x) {continue;}

        mode = SUBVAL(attribute[0].w, 10, 3);
        switch (mode)
        {
        case 0:  if (!objenabled)       {continue;} extraflag = 0;                    break;
        case 1:  if (!objenabled)       {continue;} extraflag = DOT_CSEALPHABLENDOBJ; break;
        case 2:  if (!objwindowenabled) {continue;} extraflag = DOT_OBJWINDOW;        break;
        default: continue;
        }

        shape = SUBVAL(attribute[0].w, 14, 3);
        if (shape == 3) {continue;}

        attribute[1].b.b0.b = m_OAM[objbase + 2];
        attribute[1].b.b1.b = m_OAM[objbase + 3];

        size  = SUBVAL(attribute[1].w, 14, 3);

        ypos  = attribute[0].w & 255;
        vlim  = m_dimension[shape][size][1];
        vsize = vlim;
        if (users && use2x) {vlim <<= 1;}

        vstart = ypos;
        vend   = (vstart + vlim) & 255;

        if (vend > vstart)
        {
            if ((m_VCOUNT.b < vstart) || (m_VCOUNT.b >= vend)) {continue;}
            tilerow = m_VCOUNT.b - vstart;
        }
        else
        {
            if ((m_VCOUNT.b < vstart) && (m_VCOUNT.b >= vend)) {continue;}
            tilerow = (256 - vstart) + m_VCOUNT.b;
        }
        
        xpos  = attribute[1].w & 511;
        hlim  = m_dimension[shape][size][0];
        hsize = hlim;
        if (users && use2x) {hlim <<= 1;}

        hstart = xpos;
        hend   = (hstart + hlim) & 511;

        if (hend > hstart)
        {
            if (hstart >= 240) {continue;}
            tilecol = 0;
        }
        else
        {
            if (hend   ==   0) {continue;}
            tilecol = 512 - hstart;
        }

        attribute[2].b.b0.b = m_OAM[objbase + 4];
        attribute[2].b.b1.b = m_OAM[objbase + 5];

        chr       = attribute[2].w & 1023;
        if (IsBGModeBitmap() && chr < 512) {continue;}
        usemosaic = BITTEST(attribute[0].w, 12);
        use256x1  = BITTEST(attribute[0].w, 13);
        priority  = SUBVAL(attribute[2].w, 10, 3);
        palette   = SUBVAL(attribute[2].w, 12, 0xF);

        dot      = (xpos + tilecol) & 511;
        htileofs = tilecol;        

        onedimensional = IsOBJModeOneDimensional();

        if (users)
        {
            pxbase = SUBVAL(attribute[1].w, 9, 31) << 5;

            PA.w.w0.b.b0.b = m_OAM[pxbase + 0x06];
            PA.w.w0.b.b1.b = m_OAM[pxbase + 0x07];
            PB.w.w0.b.b0.b = m_OAM[pxbase + 0x0E];
            PB.w.w0.b.b1.b = m_OAM[pxbase + 0x0F];
            PC.w.w0.b.b0.b = m_OAM[pxbase + 0x16];
            PC.w.w0.b.b1.b = m_OAM[pxbase + 0x17];
            PD.w.w0.b.b0.b = m_OAM[pxbase + 0x1E];
            PD.w.w0.b.b1.b = m_OAM[pxbase + 0x1F];

            PA.d = SIGNEX(PA.d, 15);
            PB.d = SIGNEX(PB.d, 15);
            PC.d = SIGNEX(PC.d, 15);
            PD.d = SIGNEX(PD.d, 15);

            hcenter = hlim >> 1;
            vcenter = vlim >> 1;
            hdelta  = 1;

            objhcenter = hsize >> 1;
            objvcenter = vsize >> 1;
        }
        else
        {
            if (BITTEST(attribute[1].w, 12))
            {
                tilecol = (hsize - 1) - tilecol;
                hdelta  = NEGATE(1U);
            }
            else
            {
                hdelta  = 1;
            }
            if (BITTEST(attribute[1].w, 13)) {tilerow = (vsize - 1) - tilerow;}
        }

        if (usemosaic)
        {
            hmosaic = m_hmosaic;
            vmosaic = m_vmosaic;
        }
        else
        {
            hmosaic = 1;
            vmosaic = 1;
        }

        for (u32 h = htileofs; (h < hlim) && (dot < m_screenwidth); ++h, ++dot)
        {
            mosaicrow = (tilerow / vmosaic) * vmosaic;
            mosaiccol = (tilecol / hmosaic) * hmosaic;

            tilecol += hdelta;

            if (users)
            {
                xdelta = ((mosaicrow - vcenter) * PB.d) + ((mosaiccol - hcenter) * PA.d);
                ydelta = ((mosaicrow - vcenter) * PD.d) + ((mosaiccol - hcenter) * PC.d);
                mosaicrow = ReferencePoint::FixedPointToInteger(ydelta) + (objvcenter);
                mosaiccol = ReferencePoint::FixedPointToInteger(xdelta) + (objhcenter);
                if ((mosaiccol >= hsize) || (mosaicrow >= vsize)) {continue;}
            }

            if (use256x1)
            {
                color = m_tilebase[((chr & ~1) << 5) +
                                   (onedimensional ? (((hsize >> 3) * (mosaicrow >> 3)) << 6) : ((mosaicrow & ~7) << 7)) +
                                   ((mosaicrow &  7) << 3) +
                                   ((mosaiccol & ~7) << 3) +
                                    (mosaiccol &  7)];
                pixel = Palette::GetOBJColor256x1(color);
            }
            else
            {
                color = m_tilebase[(chr << 5) +
                                   (onedimensional ? (((hsize >> 3) * (mosaicrow >> 3)) << 5) : ((mosaicrow & ~7) << 7)) +
                                   ((mosaicrow &  7) << 2) +
                                   ((mosaiccol & ~7) << 2) +
                                   ((mosaiccol &  7) >> 1)];
                color = SUBVAL(color, (mosaiccol & 1) << 2, 15);
                pixel = Palette::GetOBJColor16x16(palette, color);
            }

            if (color == 0) {continue;}

            if (mode == 2)
            {
                m_objw[dot] = m_objwflags;
            }
            else if ((m_attr[dot] == DOT_TRANSPARENT) || ((m_attr[dot] & DOT_PRIORITYBITS) > priority))
            {
                m_line[dot] = pixel;
                m_attr[dot] = DOT_OPAQUE | priority | extraflag;
            }
        }
    }
}

void OBJ::GetLine(u16 const *&line, u16 const *&attr)
{
    line = m_line;
    attr = m_attr;
}

void OBJ::GetWindowLine(u16 *&objw)
{
    objw = m_objw;
}

//-------------------------------------------------------------------------------------------------
// Procesamiento de Ventanas ----------------------------------------------------------------------
class WindowRectangular
{
private:
    t16  m_WINH;
    t16  m_WINV;
    u32  m_x1;
    u32  m_x2;
    u32  m_y1;
    u32  m_y2;
    bool m_invertx;
    bool m_inverty;
    u16  m_flags;

    void UpdateCoordinates();

public:
    void Reset();
    void RenderLine(u16 *target);
    void SetFlags(u16 flags);    
    void WriteWINH_B0(u8 byte);
    void WriteWINH_B1(u8 byte);
    void WriteWINV_B0(u8 byte);
    void WriteWINV_B1(u8 byte);
};

void WindowRectangular::UpdateCoordinates()
{
    m_invertx = m_x1 > m_x2;
    m_inverty = m_y1 > m_y2;
}

void WindowRectangular::Reset()
{
    m_WINH.w = 0;
    m_WINV.w = 0;

    m_x1 = 0;
    m_x2 = 0;
    m_y1 = 0;
    m_y2 = 0;

    m_invertx = false;
    m_inverty = false;

    m_flags = 0;
}

void WindowRectangular::RenderLine(u16 *target)
{
    if ((m_inverty && ((m_VCOUNT.b < m_y2) || (m_VCOUNT.b >= m_y1))) || (!m_inverty && ((m_VCOUNT.b >= m_y1) && (m_VCOUNT.b < m_y2))))
    {
        if (m_invertx)
        {
            for (u32 i = 0   ; i < m_x2         ; ++i) {target[i] = m_flags;}
            for (u32 i = m_x1; i < m_screenwidth; ++i) {target[i] = m_flags;}
        }
        else
        {
            for (u32 i = m_x1; i < m_x2         ; ++i) {target[i] = m_flags;}
        }
    }
}

void WindowRectangular::SetFlags(u16 flags)
{
    m_flags = flags;
}

void WindowRectangular::WriteWINH_B0(u8 byte) {m_WINH.b.b0.b = byte; m_x2 = (byte > m_screenwidth ) ? m_screenwidth  : byte; UpdateCoordinates();}
void WindowRectangular::WriteWINH_B1(u8 byte) {m_WINH.b.b1.b = byte; m_x1 = (byte > m_screenwidth ) ? m_screenwidth  : byte; UpdateCoordinates();}
void WindowRectangular::WriteWINV_B0(u8 byte) {m_WINV.b.b0.b = byte; m_y2 = (byte > m_screenheight) ? m_screenheight : byte; UpdateCoordinates();}
void WindowRectangular::WriteWINV_B1(u8 byte) {m_WINV.b.b1.b = byte; m_y1 = (byte > m_screenheight) ? m_screenheight : byte; UpdateCoordinates();}

class Window
{
private:
    static WindowRectangular m_win0;
    static WindowRectangular m_win1;

    static t16 m_WININ;
    static t16 m_WINOUT;
    static u16 m_outwflags;
    static u16 m_objwflags;
    static u16 m_win1flags;
    static u16 m_win0flags;

    static void ProcessFlags(u16 &flags, u8 byte);

public:
    static void Reset();
    static void RenderLine(u16 *target);
    static void WriteWIN0H_B0(u8 byte);
    static void WriteWIN0H_B1(u8 byte);
    static void WriteWIN1H_B0(u8 byte);
    static void WriteWIN1H_B1(u8 byte);
    static void WriteWIN0V_B0(u8 byte);
    static void WriteWIN0V_B1(u8 byte);
    static void WriteWIN1V_B0(u8 byte);
    static void WriteWIN1V_B1(u8 byte);
    static void WriteWININ_B0(u8 byte);
    static void WriteWININ_B1(u8 byte);
    static void WriteWINOUT_B0(u8 byte);
    static void WriteWINOUT_B1(u8 byte);
    static u8 ReadWININ_B0();
    static u8 ReadWININ_B1();
    static u8 ReadWINOUT_B0();
    static u8 ReadWINOUT_B1();
};

WindowRectangular Window::m_win0;
WindowRectangular Window::m_win1;

t16 Window::m_WININ;
t16 Window::m_WINOUT;
u16 Window::m_outwflags;
u16 Window::m_objwflags;
u16 Window::m_win1flags;
u16 Window::m_win0flags;

void Window::ProcessFlags(u16 &flags, u8 byte)
{
    if (BITTEST(byte, 0)) {flags |= DOT_WINDOWUSEBG0;} else {flags &= ~DOT_WINDOWUSEBG0;}
    if (BITTEST(byte, 1)) {flags |= DOT_WINDOWUSEBG1;} else {flags &= ~DOT_WINDOWUSEBG1;}
    if (BITTEST(byte, 2)) {flags |= DOT_WINDOWUSEBG2;} else {flags &= ~DOT_WINDOWUSEBG2;}
    if (BITTEST(byte, 3)) {flags |= DOT_WINDOWUSEBG3;} else {flags &= ~DOT_WINDOWUSEBG3;}
    if (BITTEST(byte, 4)) {flags |= DOT_WINDOWUSEOBJ;} else {flags &= ~DOT_WINDOWUSEOBJ;}
    if (BITTEST(byte, 5)) {flags |= DOT_WINDOWUSECSE;} else {flags &= ~DOT_WINDOWUSECSE;}
}

void Window::Reset()
{
    m_win0.Reset();
    m_win1.Reset();

    m_WININ.w  = 0;
    m_WINOUT.w = 0;

    m_outwflags = DOT_OUTSIDEWINDOW;
    m_objwflags = DOT_OBJWINDOW;
    m_win1flags = DOT_WINDOW1;
    m_win0flags = DOT_WINDOW0;

    m_win0.SetFlags(m_win0flags);
    m_win1.SetFlags(m_win1flags);
    OBJ::SetOBJWindowFlags(m_objwflags);
    OBJ::SetOutsideWindowFlags(m_outwflags);
}

void Window::RenderLine(u16 *target)
{
    if (IsWindow1Enabled()) {m_win1.RenderLine(target);}
    if (IsWindow0Enabled()) {m_win0.RenderLine(target);}
}

void Window::WriteWIN0H_B0(u8 byte) {m_win0.WriteWINH_B0(byte);}
void Window::WriteWIN0H_B1(u8 byte) {m_win0.WriteWINH_B1(byte);}
void Window::WriteWIN1H_B0(u8 byte) {m_win1.WriteWINH_B0(byte);}
void Window::WriteWIN1H_B1(u8 byte) {m_win1.WriteWINH_B1(byte);}
void Window::WriteWIN0V_B0(u8 byte) {m_win0.WriteWINV_B0(byte);}
void Window::WriteWIN0V_B1(u8 byte) {m_win0.WriteWINV_B1(byte);}
void Window::WriteWIN1V_B0(u8 byte) {m_win1.WriteWINV_B0(byte);}
void Window::WriteWIN1V_B1(u8 byte) {m_win1.WriteWINV_B1(byte);}

void Window::WriteWININ_B0(u8 byte)
{
    m_WININ.b.b0.b = byte;
    ProcessFlags(m_win0flags, byte);
    m_win0.SetFlags(m_win0flags);
}

void Window::WriteWININ_B1(u8 byte)
{
    m_WININ.b.b1.b = byte;
    ProcessFlags(m_win1flags, byte);
    m_win1.SetFlags(m_win1flags);
}

void Window::WriteWINOUT_B0(u8 byte)
{
    m_WINOUT.b.b0.b = byte;
    ProcessFlags(m_outwflags, byte);
    OBJ::SetOutsideWindowFlags(m_outwflags);
}

void Window::WriteWINOUT_B1(u8 byte)
{
    m_WINOUT.b.b1.b = byte;
    ProcessFlags(m_objwflags, byte);
    OBJ::SetOBJWindowFlags(m_objwflags);
}

u8 Window::ReadWININ_B0() {return m_WININ.b.b0.b;}
u8 Window::ReadWININ_B1() {return m_WININ.b.b1.b;}

u8 Window::ReadWINOUT_B0() {return m_WINOUT.b.b0.b;}
u8 Window::ReadWINOUT_B1() {return m_WINOUT.b.b1.b;}

//-------------------------------------------------------------------------------------------------
// Procesamiento de Efectos Especiales ------------------------------------------------------------
class ColorSpecialEffect
{
private:
    static const u16 m_1stflags[4];

    static t16  m_BLDCNT;
    static t16  m_BLDALPHA;
    static t8   m_BLDY;
    static u32  m_cse;
    static u32  m_evanum;
    static u32  m_evbnum;
    static u32  m_evynum;
    static u16  m_bgflags[4];
    static u16  m_objflags;
    static u16  m_bdflags;
    static bool m_swapgreen;

    static void UnpackColor(u16 color, u32 &red, u32 &green, u32 &blue);
    static u16 PackColor(u32 red, u32 green, u32 blue);
    static u16 AlphaBlend(u16 color1st, u16 color2nd);
    static u16 BrightnessIncrease(u16 color1st);
    static u16 BrightnessDecrease(u16 color1st);
    static void GreenSwap(u16 *target);

public:
    static void Reset();
    static void Blend(u16 *target, u16 const *line1st, u16 const *attr1st, u16 const *line2nd, u16 const *attr2nd);
    static void SetGreenSwap(bool swapgreen);
    static u16 GetBGFlags(u32 bg);
    static u16 GetOBJFlags();
    static u16 GetBackdropFlags();
    static void UpdateFlags();
    static void WriteBLDCNT_B0(u8 byte);
    static void WriteBLDCNT_B1(u8 byte);
    static void WriteBLDALPHA_B0(u8 byte);
    static void WriteBLDALPHA_B1(u8 byte);
    static void WriteBLDY(u8 byte);
    static u8 ReadBLDCNT_B0();
    static u8 ReadBLDCNT_B1();
};

const u16 ColorSpecialEffect::m_1stflags[4] =
{
    DOT_CSENONE,
    DOT_CSEALPHABLEND1ST,
    DOT_CSEMOREBRIGHT1ST,
    DOT_CSELESSBRIGHT1ST
};

t16  ColorSpecialEffect::m_BLDCNT;
t16  ColorSpecialEffect::m_BLDALPHA;
t8   ColorSpecialEffect::m_BLDY;
u32  ColorSpecialEffect::m_cse;
u32  ColorSpecialEffect::m_evanum;
u32  ColorSpecialEffect::m_evbnum;
u32  ColorSpecialEffect::m_evynum;
u16  ColorSpecialEffect::m_bgflags[4];
u16  ColorSpecialEffect::m_objflags;
u16  ColorSpecialEffect::m_bdflags;
bool ColorSpecialEffect::m_swapgreen;

void ColorSpecialEffect::UnpackColor(u16 color, u32 &red, u32 &green, u32 &blue)
{
    red   = color & 0x001F;
    green = color & 0x03E0;
    blue  = color & 0x7C00;
}

u16 ColorSpecialEffect::PackColor(u32 red, u32 green, u32 blue)
{
    return ((red & 0x001F) | (green & 0x03E0) | (blue & 0x7C00)) & 0xFFFF;
}

u16 ColorSpecialEffect::AlphaBlend(u16 color1st, u16 color2nd)
{
    u32 red[3];
    u32 green[3];
    u32 blue[3];

    UnpackColor(color1st, red[0], green[0], blue[0]);
    UnpackColor(color2nd, red[1], green[1], blue[1]);

    red[2]   = ((red[0]   * m_evanum) >> 4) + ((red[1]   * m_evbnum) >> 4);
    green[2] = ((green[0] * m_evanum) >> 4) + ((green[1] * m_evbnum) >> 4);
    blue[2]  = ((blue[0]  * m_evanum) >> 4) + ((blue[1]  * m_evbnum) >> 4);

    if (red[2]   > 0x001F) {red[2]   = 0x001F;}
    if (green[2] > 0x03E0) {green[2] = 0x03E0;}
    if (blue[2]  > 0x7C00) {blue[2]  = 0x7C00;}

    return PackColor(red[2], green[2], blue[2]);
}

u16 ColorSpecialEffect::BrightnessIncrease(u16 color1st)
{
    u32 red;
    u32 green;
    u32 blue;

    UnpackColor(color1st, red, green, blue);

    red   += ((0x001F - red  ) * m_evynum) >> 4;
    green += ((0x03E0 - green) * m_evynum) >> 4;
    blue  += ((0x7C00 - blue ) * m_evynum) >> 4;

    return PackColor(red, green, blue);
}

u16 ColorSpecialEffect::BrightnessDecrease(u16 color1st)
{
    u32 red;
    u32 green;
    u32 blue;

    UnpackColor(color1st, red, green, blue);

    red   -= (red   * m_evynum) >> 4;
    green -= (green * m_evynum) >> 4;
    blue  -= (blue  * m_evynum) >> 4;

    return PackColor(red, green, blue);
}

void ColorSpecialEffect::GreenSwap(u16 *target)
{
    u32 red[2];
    u32 green[2];
    u32 blue[2];

    for (u32 i = 0; i < m_screenwidth; i += 2)
    {
        UnpackColor(target[i + 0], red[0], green[0], blue[0]);
        UnpackColor(target[i + 1], red[1], green[1], blue[1]);
        target[i + 0] = PackColor(red[0], green[1], blue[0]);
        target[i + 1] = PackColor(red[1], green[0], blue[1]);
    }
}

void ColorSpecialEffect::Reset()
{
    m_BLDCNT.w   = 0;
    m_BLDALPHA.w = 0;
    m_BLDY.b     = 0;

    m_cse      = 0;
    m_evanum   = 0;
    m_evbnum   = 0;
    m_evynum   = 0;

    m_bgflags[0] = 0;
    m_bgflags[1] = 0;
    m_bgflags[2] = 0;
    m_bgflags[3] = 0;
    m_objflags   = 0;
    m_bdflags    = 0;

    m_swapgreen = false;
}

void ColorSpecialEffect::Blend(u16 *target, u16 const *line1st, u16 const *attr1st, u16 const *line2nd, u16 const *attr2nd)
{
    for (u32 i = 0; i < m_screenwidth; ++i)
    {
        if (((attr1st[i] & DOT_CSEALPHABLENDALL) != 0) && ((attr2nd[i] & DOT_CSEALPHABLEND2ND) != 0))
        {
            target[i] = AlphaBlend(line1st[i], line2nd[i]);
        }
        else if ((attr1st[i] & DOT_CSEMOREBRIGHT1ST) != 0)
        {
            target[i] = BrightnessIncrease(line1st[i]);
        }
        else if ((attr1st[i] & DOT_CSELESSBRIGHT1ST) != 0)
        {
            target[i] = BrightnessDecrease(line1st[i]);
        }
        else
        {
            target[i] = line1st[i];
        }
    }

    if (m_swapgreen) {GreenSwap(target);}
}

void ColorSpecialEffect::SetGreenSwap(bool swapgreen)
{
    m_swapgreen = swapgreen;
}

u16 ColorSpecialEffect::GetBGFlags(u32 bg) {return m_bgflags[bg & 3];}
u16 ColorSpecialEffect::GetOBJFlags()      {return m_objflags;}
u16 ColorSpecialEffect::GetBackdropFlags() {return m_bdflags ;}

void ColorSpecialEffect::UpdateFlags()
{
    u16 flag1st;

    m_cse   = SUBVAL(m_BLDCNT.w, 6, 3);
    flag1st = m_1stflags[m_cse];

    m_bgflags[0] = DOT_CSENONE;
    m_bgflags[1] = DOT_CSENONE;
    m_bgflags[2] = DOT_CSENONE;
    m_bgflags[3] = DOT_CSENONE;
    m_objflags   = DOT_CSENONE;
    m_bdflags    = DOT_CSENONE;

    if (BITTEST(m_BLDCNT.w, 0))  {m_bgflags[0] |= flag1st;}
    if (BITTEST(m_BLDCNT.w, 1))  {m_bgflags[1] |= flag1st;}
    if (BITTEST(m_BLDCNT.w, 2))  {m_bgflags[2] |= flag1st;}
    if (BITTEST(m_BLDCNT.w, 3))  {m_bgflags[3] |= flag1st;}
    if (BITTEST(m_BLDCNT.w, 4))  {m_objflags   |= flag1st;}
    if (BITTEST(m_BLDCNT.w, 5))  {m_bdflags    |= flag1st;}

    if (BITTEST(m_BLDCNT.w, 8))  {m_bgflags[0] |= DOT_CSEALPHABLEND2ND;}
    if (BITTEST(m_BLDCNT.w, 9))  {m_bgflags[1] |= DOT_CSEALPHABLEND2ND;}
    if (BITTEST(m_BLDCNT.w, 10)) {m_bgflags[2] |= DOT_CSEALPHABLEND2ND;}
    if (BITTEST(m_BLDCNT.w, 11)) {m_bgflags[3] |= DOT_CSEALPHABLEND2ND;}
    if (BITTEST(m_BLDCNT.w, 12)) {m_objflags   |= DOT_CSEALPHABLEND2ND;}
    if (BITTEST(m_BLDCNT.w, 13)) {m_bdflags    |= DOT_CSEALPHABLEND2ND;}
}

void ColorSpecialEffect::WriteBLDCNT_B0(u8 byte) {m_BLDCNT.b.b0.b = byte; UpdateFlags();}
void ColorSpecialEffect::WriteBLDCNT_B1(u8 byte) {m_BLDCNT.b.b1.b = byte; UpdateFlags();}

void ColorSpecialEffect::WriteBLDALPHA_B0(u8 byte) {m_BLDALPHA.b.b0.b = byte; u32 eva = SUBVAL(byte, 0, 31); if (eva > 16) {eva = 16;} m_evanum = eva;}
void ColorSpecialEffect::WriteBLDALPHA_B1(u8 byte) {m_BLDALPHA.b.b1.b = byte; u32 evb = SUBVAL(byte, 0, 31); if (evb > 16) {evb = 16;} m_evbnum = evb;}
void ColorSpecialEffect::WriteBLDY(u8 byte)        {m_BLDY.b          = byte; u32 evy = SUBVAL(byte, 0, 31); if (evy > 16) {evy = 16;} m_evynum = evy;}

u8 ColorSpecialEffect::ReadBLDCNT_B0() {return m_BLDCNT.b.b0.b;}
u8 ColorSpecialEffect::ReadBLDCNT_B1() {return m_BLDCNT.b.b1.b;}

//-------------------------------------------------------------------------------------------------
// BG Modo Text -----------------------------------------------------------------------------------
class BGText
{
private:
    u16  m_line[m_screenwidth];
    u16  m_attr[m_screenwidth];    
    t16  m_BGHOFS;
    t16  m_BGVOFS;
    u8  *m_tilebase;
    u8  *m_mapbase;
    bool m_use256x1;
    u32  m_hmask;
    u32  m_vmask;
    bool m_usemosaic;
    u32  m_hmosaic;
    u32  m_vmosaic;

public:
    void Reset();
    void SetMosaic(u32 h, u32 v);
    void RenderLine();
    void GetLine(u16 const *&line, u16 const *&attr);    
    void WriteBGCNT_B0(u8 byte);
    void WriteBGCNT_B1(u8 byte);
    void WriteBGHOFS_B0(u8 byte);
    void WriteBGHOFS_B1(u8 byte);
    void WriteBGVOFS_B0(u8 byte);
    void WriteBGVOFS_B1(u8 byte);
};

void BGText::Reset()
{
    m_tilebase = &m_VRAM[0];
    m_mapbase  = &m_VRAM[0];

    m_use256x1 = false;

    m_hmask   = 255;
    m_vmask   = 255;
    m_hmosaic = 1;
    m_vmosaic = 1;
}

void BGText::SetMosaic(u32 h, u32 v)
{
    m_hmosaic = h;
    m_vmosaic = v;
}

void BGText::RenderLine()
{
    u32  vpx      = (m_BGVOFS.w + m_VCOUNT.b) & m_vmask;
    u32  vscx256  = vpx & BIT(8) & m_vmask;
    u32  vmap     = (vpx & 0xF8) << 2;
    u32  vtileofs = vpx & 7;
    u8  *mapbase  = &m_mapbase[(vmap + (vscx256 << 2) + ((vscx256 & m_hmask) << 2)) << 1];
    u32  dot      = 0;

    u32  hpx;
    u32  hscx256;
    u32  hmap;
    u32  htileofs;
    u32  mosaicrow;
    u32  mosaiccol;
    u32  hmosaic;
    u32  vmosaic;
    t16  mapentry;
    u32  chr;
    u32  tilerow;
    u32  tilecol;
    u32  hdelta;
    u8  *tilebase;
    u32  color;
    u32  palette;
    u32  mapindex;
    u16  pixel;

    if (m_usemosaic)
    {
        hmosaic = m_hmosaic;
        vmosaic = m_vmosaic;
    }
    else
    {
        hmosaic = 1;
        vmosaic = 1;
    }

    for (u32 i = 0; i < m_screenwidth; ++i) {m_attr[i] = DOT_TRANSPARENT;}

    for (;;)
    {
        hpx      = (m_BGHOFS.w + dot) & m_hmask;
        hscx256  = hpx & BIT(8) & m_hmask;
        hmap     = (hpx >> 3) & 31;
        htileofs = hpx & 7;
        mapindex = (hmap + (hscx256 << 2)) << 1;

        mapentry.b.b0.b = mapbase[mapindex + 0];
        mapentry.b.b1.b = mapbase[mapindex + 1];

        chr      = mapentry.w & 1023;
        if (BITTEST(mapentry.w, 10))
        {
            tilecol = 7 - htileofs;
            hdelta  = NEGATE(1U);
        }
        else
        {
            tilecol = htileofs;
            hdelta  = 1;
        }
        tilerow  = BITTEST(mapentry.w, 11) ? (7 - vtileofs) : vtileofs;

        mosaicrow = (tilerow / vmosaic) * vmosaic;
        tilebase  = &m_tilebase[m_use256x1 ? ((chr << 6) + (mosaicrow << 3)) : ((chr << 5) + (mosaicrow << 2))];
        palette   = SUBVAL(mapentry.w, 12, 0xF);

        for (u32 h = htileofs; h < 8; ++h)
        {
            mosaiccol = (tilecol / hmosaic) * hmosaic;
            tilecol += hdelta;

            if (m_use256x1)
            {
                color = tilebase[mosaiccol];
                pixel = Palette::GetBGColor256x1(color);
            }
            else
            {
                color = SUBVAL(tilebase[mosaiccol >> 1], (mosaiccol & 1) << 2, 15);
                pixel = Palette::GetBGColor16x16(palette, color);
            }

            if (color != 0)
            {
                m_line[dot] = pixel;
                m_attr[dot] = DOT_OPAQUE;
            }

            if (++dot == m_screenwidth) {return;}
        }
    }
}

void BGText::GetLine(u16 const *&line, u16 const *&attr)
{
    line = m_line;
    attr = m_attr;
}

void BGText::WriteBGCNT_B0(u8 byte)
{
    m_tilebase  = &m_VRAM[SUBVAL(byte, 2, 0x3) * 16 * 1024];
    m_usemosaic = BITTEST(byte, 6);
    m_use256x1  = BITTEST(byte, 7);
}

void BGText::WriteBGCNT_B1(u8 byte)
{
    m_mapbase = &m_VRAM[SUBVAL(byte, 0, 0x1F) * 2 * 1024];
    switch (SUBVAL(byte, 6, 0x3))
    {
    case 0:  m_hmask = 255; m_vmask = 255; break;
    case 1:  m_hmask = 511; m_vmask = 255; break;
    case 2:  m_hmask = 255; m_vmask = 511; break;
    default: m_hmask = 511; m_vmask = 511;
    }
}

void BGText::WriteBGHOFS_B0(u8 byte) {m_BGHOFS.b.b0.b = byte;}
void BGText::WriteBGHOFS_B1(u8 byte) {m_BGHOFS.b.b1.b = byte;}
void BGText::WriteBGVOFS_B0(u8 byte) {m_BGVOFS.b.b0.b = byte;}
void BGText::WriteBGVOFS_B1(u8 byte) {m_BGVOFS.b.b1.b = byte;}

//-------------------------------------------------------------------------------------------------
// Puntos de referencia para BG con parametros A, B, C y D ----------------------------------------
class BGReferencePoint
{
protected:
    u16  m_line[m_screenwidth];
    u16  m_attr[m_screenwidth];
    t32  m_BGX;
    t32  m_BGY;
    t16  m_BGPA;
    t16  m_BGPB;
    t16  m_BGPC;
    t16  m_BGPD;
    bool m_wrap;
    u32  m_X;
    u32  m_Y;
    u32  m_dx;
    u32  m_dmx;
    u32  m_dy;
    u32  m_dmy;
    bool m_usemosaic;
    u32  m_hmosaic;
    u32  m_vmosaic;

    void UpdateBGX();
    void UpdateBGY();
    void UpdatePA();
    void UpdatePB();
    void UpdatePC();
    void UpdatePD();

public:
    void Reset();
    void SetMosaic(u32 h, u32 v);
    void GetLine(u16 const *&line, u16 const *&attr);    
    void OnLeaveVblank();
    void WriteBGCNT_B0(u8 byte);
    void WriteBGCNT_B1(u8 byte);
    void WriteBGX_L_B0(u8 byte);
    void WriteBGX_L_B1(u8 byte);
    void WriteBGX_H_B0(u8 byte);
    void WriteBGX_H_B1(u8 byte);
    void WriteBGY_L_B0(u8 byte);
    void WriteBGY_L_B1(u8 byte);
    void WriteBGY_H_B0(u8 byte);
    void WriteBGY_H_B1(u8 byte);
    void WriteBGPA_B0(u8 byte);
    void WriteBGPA_B1(u8 byte);
    void WriteBGPB_B0(u8 byte);
    void WriteBGPB_B1(u8 byte);
    void WriteBGPC_B0(u8 byte);
    void WriteBGPC_B1(u8 byte);
    void WriteBGPD_B0(u8 byte);
    void WriteBGPD_B1(u8 byte);
};

void BGReferencePoint::UpdateBGX() {m_X   = SIGNEX(m_BGX.d,  27);}
void BGReferencePoint::UpdateBGY() {m_Y   = SIGNEX(m_BGY.d,  27);}
void BGReferencePoint::UpdatePA()  {m_dx  = SIGNEX(m_BGPA.w, 15);}
void BGReferencePoint::UpdatePB()  {m_dmx = SIGNEX(m_BGPB.w, 15);}
void BGReferencePoint::UpdatePC()  {m_dy  = SIGNEX(m_BGPC.w, 15);}
void BGReferencePoint::UpdatePD()  {m_dmy = SIGNEX(m_BGPD.w, 15);}

void BGReferencePoint::Reset()
{
    m_wrap = false;

    m_BGX.d  = 0;
    m_BGY.d  = 0;
    m_BGPA.w = 0x0100;
    m_BGPB.w = 0;
    m_BGPC.w = 0;
    m_BGPD.w = 0x0100;
    m_X      = 0;
    m_Y      = 0;
    m_dx     = 0x0100;
    m_dmx    = 0;
    m_dy     = 0;
    m_dmy    = 0x0100;

    m_usemosaic = false;

    m_hmosaic = 1;
    m_vmosaic = 1;
}

void BGReferencePoint::SetMosaic(u32 h, u32 v)
{
    m_hmosaic = h;
    m_vmosaic = v;
}

void BGReferencePoint::GetLine(u16 const *&line, u16 const *&attr)
{
    line = m_line;
    attr = m_attr;
}

void BGReferencePoint::OnLeaveVblank()
{
    UpdateBGX();
    UpdateBGY();
}

void BGReferencePoint::WriteBGCNT_B0(u8 byte)
{
    m_usemosaic = BITTEST(byte, 6);
}

void BGReferencePoint::WriteBGCNT_B1(u8 byte)
{
    m_wrap = BITTEST(byte, 5);
}

void BGReferencePoint::WriteBGX_L_B0(u8 byte) {m_BGX.w.w0.b.b0.b = byte; UpdateBGX();}
void BGReferencePoint::WriteBGX_L_B1(u8 byte) {m_BGX.w.w0.b.b1.b = byte; UpdateBGX();}
void BGReferencePoint::WriteBGX_H_B0(u8 byte) {m_BGX.w.w1.b.b0.b = byte; UpdateBGX();}
void BGReferencePoint::WriteBGX_H_B1(u8 byte) {m_BGX.w.w1.b.b1.b = byte; UpdateBGX();}
void BGReferencePoint::WriteBGY_L_B0(u8 byte) {m_BGY.w.w0.b.b0.b = byte; UpdateBGY();}
void BGReferencePoint::WriteBGY_L_B1(u8 byte) {m_BGY.w.w0.b.b1.b = byte; UpdateBGY();}
void BGReferencePoint::WriteBGY_H_B0(u8 byte) {m_BGY.w.w1.b.b0.b = byte; UpdateBGY();}
void BGReferencePoint::WriteBGY_H_B1(u8 byte) {m_BGY.w.w1.b.b1.b = byte; UpdateBGY();}
void BGReferencePoint::WriteBGPA_B0(u8 byte)  {m_BGPA.b.b0.b     = byte; UpdatePA();}
void BGReferencePoint::WriteBGPA_B1(u8 byte)  {m_BGPA.b.b1.b     = byte; UpdatePA();}
void BGReferencePoint::WriteBGPB_B0(u8 byte)  {m_BGPB.b.b0.b     = byte; UpdatePB();}
void BGReferencePoint::WriteBGPB_B1(u8 byte)  {m_BGPB.b.b1.b     = byte; UpdatePB();}
void BGReferencePoint::WriteBGPC_B0(u8 byte)  {m_BGPC.b.b0.b     = byte; UpdatePC();}
void BGReferencePoint::WriteBGPC_B1(u8 byte)  {m_BGPC.b.b1.b     = byte; UpdatePC();}
void BGReferencePoint::WriteBGPD_B0(u8 byte)  {m_BGPD.b.b0.b     = byte; UpdatePD();}
void BGReferencePoint::WriteBGPD_B1(u8 byte)  {m_BGPD.b.b1.b     = byte; UpdatePD();}

//-------------------------------------------------------------------------------------------------
// BG Modo Rotation and Scaling -------------------------------------------------------------------
class BGRotationAndScaling : public BGReferencePoint
{
private:
    u8  *m_mapbase;
    u8  *m_tilebase;
    u32  m_length;
    u32  m_mask;
    u32  m_rowshift;

public:
    void Reset();
    void RenderLine();
    void WriteBGCNT_B0(u8 byte);
    void WriteBGCNT_B1(u8 byte);
};

void BGRotationAndScaling::Reset()
{
    m_tilebase = &m_VRAM[0];
    m_mapbase  = &m_VRAM[0];
    m_length   = 128;
    m_mask     = 127;
    m_rowshift = 4;    
}

void BGRotationAndScaling::RenderLine()
{
    u32 fY = m_Y;
    u32 fX = m_X;

    u32 vpx;
    u32 hpx;
    u32 vmap;
    u32 hmap;
    u32 vtileofs;
    u32 htileofs;
    u32 color;
    u32 mask;
    u32 hmosaic;
    u32 vmosaic;
    u32 dmyofs;
    u32 dmxofs;
    u32 dyofs;
    u32 dxofs;
    u32 vmodulus;
    u32 hmodulus;

    for (u32 i = 0; i < m_screenwidth; ++i) {m_attr[i] = DOT_TRANSPARENT;}

    m_Y += m_dmy;
    m_X += m_dmx;

    if (m_usemosaic)
    {
        hmosaic = m_hmosaic;
        vmosaic = m_vmosaic;
    }
    else
    {
        hmosaic = 1;
        vmosaic = 1;
    }

    vmodulus = m_VCOUNT.b % vmosaic;
    dmyofs   = vmodulus * m_dmy;
    dmxofs   = vmodulus * m_dmx;

    mask = m_wrap ? m_mask : ~0;

    for (u32 dot = 0; dot < m_screenwidth; ++dot)
    {
        hmodulus = dot % hmosaic;
        dyofs    = hmodulus * m_dy;
        dxofs    = hmodulus * m_dx;

        vpx = ReferencePoint::FixedPointToInteger(fY - dyofs - dmyofs) & mask;
        hpx = ReferencePoint::FixedPointToInteger(fX - dxofs - dmxofs) & mask;        

        fY += m_dy;
        fX += m_dx;

        if (!m_wrap && ((vpx >= m_length) || (hpx >= m_length))) {continue;}

        vmap     = (vpx >> 3) << m_rowshift;
        hmap     = (hpx >> 3);
        vtileofs = vpx & 7;
        htileofs = hpx & 7;
        color    = m_tilebase[(m_mapbase[vmap + hmap] << 6) + (vtileofs << 3) + htileofs];
    
        if (color != 0)
        {
            m_line[dot] = Palette::GetBGColor256x1(color);
            m_attr[dot] = DOT_OPAQUE;
        }
    }
}

void BGRotationAndScaling::WriteBGCNT_B0(u8 byte)
{
    m_tilebase = &m_VRAM[SUBVAL(byte, 2, 3) * 16 * 1024];
}

void BGRotationAndScaling::WriteBGCNT_B1(u8 byte)
{
    u32 s      = SUBVAL(byte, 6, 3);
    m_mapbase  = &m_VRAM[SUBVAL(byte, 0, 31) * 2 * 1024];  
    m_length   = 128 << s;
    m_mask     = m_length - 1;
    m_rowshift = 4 + s;
}

//-------------------------------------------------------------------------------------------------
// BG Modo Bitmap ---------------------------------------------------------------------------------
class BGBitmap : public BGRotationAndScaling
{
private:
    u32 m_offset;

    void RenderLine(u32 offset, u32 vmax, u32 hmax, bool use256x1);

public:
    void Reset();
    void RenderLineMode3();
    void RenderLineMode4();
    void RenderLineMode5();
    void SetFrame(u32 frame);
};

void BGBitmap::RenderLine(u32 offset, u32 vmax, u32 hmax, bool use256x1)
{
    u32 fY = m_Y;
    u32 fX = m_X;

    u32 vpx;
    u32 hpx;
    u32 dotbase;
    t16 color;
    u32 entry;
    u32 hmosaic;
    u32 vmosaic;
    u32 dmyofs;
    u32 dmxofs;
    u32 dyofs;
    u32 dxofs;
    u32 vmodulus;
    u32 hmodulus;

    for (u32 i = 0; i < m_screenwidth; ++i) {m_attr[i] = DOT_TRANSPARENT;}

    m_Y += m_dmy;
    m_X += m_dmx;

    if (m_usemosaic)
    {
        hmosaic = m_hmosaic;
        vmosaic = m_vmosaic;
    }
    else
    {
        hmosaic = 1;
        vmosaic = 1;
    }

    vmodulus = m_VCOUNT.b % vmosaic;
    dmyofs   = vmodulus * m_dmy;
    dmxofs   = vmodulus * m_dmx;

    for (u32 dot = 0; dot < m_screenwidth; ++dot)
    {
        hmodulus = dot % hmosaic;
        dyofs    = hmodulus * m_dy;
        dxofs    = hmodulus * m_dx;

        vpx = ReferencePoint::FixedPointToInteger(fY - dyofs - dmyofs);
        hpx = ReferencePoint::FixedPointToInteger(fX - dxofs - dmxofs);

        fY += m_dy;
        fX += m_dx;

        if ((vpx >= vmax) || (hpx >= hmax)) {continue;}

        dotbase = (vpx * hmax) + hpx;

        if (use256x1)
        {
            dotbase += offset;
            entry = m_VRAM[dotbase];
            if (entry == 0) {continue;}
            color.w = Palette::GetBGColor256x1(entry);            
        }
        else
        {
            dotbase <<= 1;
            dotbase += offset;
            color.b.b0.b = m_VRAM[dotbase + 0];
            color.b.b1.b = m_VRAM[dotbase + 1];
        }

        m_line[dot] = color.w;
        m_attr[dot] = DOT_OPAQUE;
    }
}

void BGBitmap::Reset() {m_offset = 0x0000;}

void BGBitmap::RenderLineMode3() {RenderLine(0x0000,   160, 240, false);}
void BGBitmap::RenderLineMode4() {RenderLine(m_offset, 160, 240, true);}
void BGBitmap::RenderLineMode5() {RenderLine(m_offset, 128, 160, false);}

void BGBitmap::SetFrame(u32 frame) {m_offset = (frame & 1) * 0xA000;}

//-------------------------------------------------------------------------------------------------
// Orden y numero de capa para cada BG ------------------------------------------------------------
class BGControl
{
private:
    static u32 m_bgorder[2][4];

    t16  m_BGCNT;
    u32  m_priority;
    u32  m_bgnumber;

public:
    static void ResetOrder();
    static void NotifyPrioritySet(u32 bg, u32 priority);
    static u32 const *GetBGOrder();

    BGControl(u32 bgnumber);
    void Reset();
    u32 GetPriority();
    void WriteBGCNT_B0(u8 byte);
    void WriteBGCNT_B1(u8 byte);
    u8 ReadBGCNT_B0();
    u8 ReadBGCNT_B1();
};

u32 BGControl::m_bgorder[2][4];

void BGControl::ResetOrder()
{
    m_bgorder[0][0] = 0;
    m_bgorder[1][0] = 0;
    m_bgorder[0][1] = 1;
    m_bgorder[1][1] = 0;
    m_bgorder[0][2] = 2;
    m_bgorder[1][2] = 0;
    m_bgorder[0][3] = 3;
    m_bgorder[1][3] = 0;
}

u32 const *BGControl::GetBGOrder() {return m_bgorder[0];}

BGControl::BGControl(u32 bgnumber) {m_bgnumber = bgnumber & 3;}

void BGControl::Reset()
{
    m_BGCNT.w  = 0;
    m_priority = 0;
}

u32 BGControl::GetPriority() {return m_priority;}

void BGControl::WriteBGCNT_B0(u8 byte)
{
    u32 nextpriority;
    m_BGCNT.b.b0.b = byte;
    nextpriority   = SUBVAL(byte, 0, 3);
    if (nextpriority != m_priority) {NotifyPrioritySet(m_bgnumber, nextpriority);}
    m_priority     = nextpriority;
}

void BGControl::WriteBGCNT_B1(u8 byte)
{
    m_BGCNT.b.b1.b = byte;
}

u8 BGControl::ReadBGCNT_B0() {return m_BGCNT.b.b0.b;}
u8 BGControl::ReadBGCNT_B1() {return m_BGCNT.b.b1.b;}

//-------------------------------------------------------------------------------------------------
// Procesamiento de BG ----------------------------------------------------------------------------
void BGControl::NotifyPrioritySet(u32 bg, u32 priority)
{
    u32 oldlayerpos;
    u32 insert;

    for (oldlayerpos = 0; (oldlayerpos < 4) && (m_bgorder[0][oldlayerpos] != bg); ++oldlayerpos);

    switch (oldlayerpos)
    {
    case 0:
        m_bgorder[0][0] = m_bgorder[0][1];
        m_bgorder[1][0] = m_bgorder[1][1];
    case 1:
        m_bgorder[0][1] = m_bgorder[0][2];
        m_bgorder[1][1] = m_bgorder[1][2];
    case 2:
        m_bgorder[0][2] = m_bgorder[0][3];
        m_bgorder[1][2] = m_bgorder[1][3];
    case 3:
        break;
    default:
        return;
    }

    for (insert = 0; insert < 3; ++insert)
    {
        if (m_bgorder[1][insert] < priority) {continue;}
        if (m_bgorder[1][insert] > priority) {break;}
        if (m_bgorder[0][insert] < bg      ) {continue;}
        break;
    }

    switch (insert)
    {
    case 0:
        m_bgorder[0][3] = m_bgorder[0][2];
        m_bgorder[1][3] = m_bgorder[1][2];
        m_bgorder[0][2] = m_bgorder[0][1];
        m_bgorder[1][2] = m_bgorder[1][1];
        m_bgorder[0][1] = m_bgorder[0][0];
        m_bgorder[1][1] = m_bgorder[1][0];
        break;
    case 1:
        m_bgorder[0][3] = m_bgorder[0][2];
        m_bgorder[1][3] = m_bgorder[1][2];
        m_bgorder[0][2] = m_bgorder[0][1];
        m_bgorder[1][2] = m_bgorder[1][1];
        break;
    case 2:
        m_bgorder[0][3] = m_bgorder[0][2];
        m_bgorder[1][3] = m_bgorder[1][2];
        break;
    case 3:
        break;
    default:
        return;
    }

    m_bgorder[0][insert] = bg;
    m_bgorder[1][insert] = priority;
}

//-------------------------------------------------------------------------------------------------

class BG0 : public BGControl, public BGText
{
public:
    BG0();
};

class BG1 : public BGControl, public BGText
{
public:
    BG1();
};

class BG2 : public BGControl, public BGText, public BGBitmap
{
public:
    BG2();
};

class BG3 : public BGControl, public BGText, public BGRotationAndScaling
{
public:
    BG3();
};

BG0::BG0() : BGControl(0) {}
BG1::BG1() : BGControl(1) {}
BG2::BG2() : BGControl(2) {}
BG3::BG3() : BGControl(3) {}

BG0 m_bg0;
BG1 m_bg1;
BG2 m_bg2;
BG3 m_bg3;

class Painter
{
private:
    static u16  m_linebuffer[2][m_screenwidth];
    static u16  m_attrbuffer[2][m_screenwidth];
    static u16 *m_winx;
    static void (*RenderLineBG)();

    static void WriteBackRow(u32 dot, u16 color, u16 attr);
    static void WriteFrontRow(u32 dot, u16 color, u16 attr);
    static void WriteBGFront(u16 const *line, u16 const *attr, u16 xattr, u16 xwinx);
    static void RenderLineBGMode0();
    static void RenderLineBGMode1();
    static void RenderLineBGMode2();
    static void RenderLineBGMode3();
    static void RenderLineBGMode4();
    static void RenderLineBGMode5();
    
public:
    static void RenderLine();
    static void SetBGMode(u32 mode);
};

u16    Painter::m_linebuffer[2][m_screenwidth];
u16    Painter::m_attrbuffer[2][m_screenwidth];
u16   *Painter::m_winx;
void (*Painter::RenderLineBG)();

void Painter::WriteBackRow(u32 dot, u16 color, u16 attr)
{
    m_linebuffer[1][dot] = color;
    m_attrbuffer[1][dot] = attr;
}

void Painter::WriteFrontRow(u32 dot, u16 color, u16 attr)
{
    WriteBackRow(dot, m_linebuffer[0][dot], m_attrbuffer[0][dot]);
    m_linebuffer[0][dot] = color;
    m_attrbuffer[0][dot] = attr;
}

void Painter::WriteBGFront(u16 const *line, u16 const *attr, u16 xattr, u16 xwinx)
{
    for (u32 i = 0; i < m_screenwidth; ++i) {if (((attr[i] & DOT_TRANSPARENT) != DOT_TRANSPARENT) && ((m_winx[i] & xwinx) == xwinx)) {WriteFrontRow(i, line[i], attr[i] | xattr);}}
}

void Painter::RenderLineBGMode0()
{
    u32 const *bgorder = BGControl::GetBGOrder();
    u16 const *line;
    u16 const *attr;
    u16 xattr;
    u16 xwinx;

    u32 layer = 4;
    u32 i = 3;

    do
    {
        switch (bgorder[--layer])
        {
        case 0:
            if (!IsBGEnabled(0)) {continue;}
            m_bg0.RenderLine();
            m_bg0.GetLine(line, attr);
            xattr = (m_bg0.GetPriority() | ColorSpecialEffect::GetBGFlags(0)) & 0xFFFF;
            
            xwinx = DOT_WINDOWUSEBG0;
            break;
        case 1:
            if (!IsBGEnabled(1)) {continue;}
            m_bg1.RenderLine();
            m_bg1.GetLine(line, attr);
            xattr = (m_bg1.GetPriority() | ColorSpecialEffect::GetBGFlags(1)) & 0xFFFF;
            
            xwinx = DOT_WINDOWUSEBG1;
            break;
        case 2:
            if (!IsBGEnabled(2)) {continue;}

            m_bg2.BGText::RenderLine();
            m_bg2.BGText::GetLine(line, attr);

            xattr = (m_bg2.GetPriority() | ColorSpecialEffect::GetBGFlags(2)) & 0xFFFF;

            xwinx = DOT_WINDOWUSEBG2;
            break;
        case 3:
            if (!IsBGEnabled(3)) {continue;}

            m_bg3.BGText::RenderLine();
            m_bg3.BGText::GetLine(line, attr);

            xattr = (m_bg3.GetPriority() | ColorSpecialEffect::GetBGFlags(3)) & 0xFFFF;

            xwinx = DOT_WINDOWUSEBG3;
            break;
        }

        WriteBGFront(line, attr, xattr, xwinx);
    }
    while (layer != 0);
}

void Painter::RenderLineBGMode1()
{
    u32 const *bgorder = BGControl::GetBGOrder();
    u16 const *line;
    u16 const *attr;
    u16 xattr;
    u16 xwinx;

    u32 layer = 4;

    do
    {
        switch (bgorder[--layer])
        {
        case 0:
            if (!IsBGEnabled(0)) {continue;}
            m_bg0.RenderLine();
            m_bg0.GetLine(line, attr);
            xattr = (m_bg0.GetPriority() | ColorSpecialEffect::GetBGFlags(0)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG0;
            break;
        case 1:
            if (!IsBGEnabled(1)) {continue;}
            m_bg1.RenderLine();
            m_bg1.GetLine(line, attr);
            xattr = (m_bg1.GetPriority() | ColorSpecialEffect::GetBGFlags(1)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG1;
            break;
        case 2:
            if (!IsBGEnabled(2)) {continue;}

            m_bg2.BGRotationAndScaling::RenderLine();
            m_bg2.BGRotationAndScaling::GetLine(line, attr);

            xattr = (m_bg2.GetPriority() | ColorSpecialEffect::GetBGFlags(2)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG2;
            break;
        case 3:
            continue;
            break;
        }

        WriteBGFront(line, attr, xattr, xwinx);
    }
    while (layer != 0);
}

void Painter::RenderLineBGMode2()
{
    u32 const *bgorder = BGControl::GetBGOrder();
    u16 const *line;
    u16 const *attr;
    u16 xattr;
    u16 xwinx;

    u32 layer = 4;

    do
    {
        switch (bgorder[--layer])
        {
        case 0:
            continue;
            break;
        case 1:
            continue;
            break;
        case 2:
            if (!IsBGEnabled(2)) {continue;}

            m_bg2.BGRotationAndScaling::RenderLine();
            m_bg2.BGRotationAndScaling::GetLine(line, attr);

            xattr = (m_bg2.GetPriority() | ColorSpecialEffect::GetBGFlags(2)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG2;
            break;
        case 3:
            if (!IsBGEnabled(3)) {continue;}

            m_bg3.BGRotationAndScaling::RenderLine();
            m_bg3.BGRotationAndScaling::GetLine(line, attr);

            xattr = (m_bg3.GetPriority() | ColorSpecialEffect::GetBGFlags(3)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG3;
            break;
        }

        WriteBGFront(line, attr, xattr, xwinx);
    }
    while (layer != 0);
}

void Painter::RenderLineBGMode3()
{
    u32 const *bgorder = BGControl::GetBGOrder();
    u16 const *line;
    u16 const *attr;
    u16 xattr;
    u16 xwinx;

    u32 layer = 4;

    do
    {
        switch (bgorder[--layer])
        {
        case 0:
            continue;
            break;
        case 1:
            continue;
            break;
        case 2:
            if (!IsBGEnabled(2)) {continue;}

            m_bg2.BGBitmap::RenderLineMode3();
            m_bg2.BGBitmap::GetLine(line, attr);

            xattr = (m_bg2.GetPriority() | ColorSpecialEffect::GetBGFlags(2)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG2;
            break;
        case 3:
            continue;
            break;
        }

        WriteBGFront(line, attr, xattr, xwinx);
    }
    while (layer != 0);
}

void Painter::RenderLineBGMode4()
{
    u32 const *bgorder = BGControl::GetBGOrder();
    u16 const *line;
    u16 const *attr;
    u16 xattr;
    u16 xwinx;

    u32 layer = 4;

    do
    {
        switch (bgorder[--layer])
        {
        case 0:
            continue;
            break;
        case 1:
            continue;
            break;
        case 2:
            if (!IsBGEnabled(2)) {continue;}

            m_bg2.BGBitmap::RenderLineMode4();
            m_bg2.BGBitmap::GetLine(line, attr);

            xattr = (m_bg2.GetPriority() | ColorSpecialEffect::GetBGFlags(2)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG2;
            break;
        case 3:
            continue;
            break;
        }

        WriteBGFront(line, attr, xattr, xwinx);
    }
    while (layer != 0);
}

void Painter::RenderLineBGMode5()
{
    u32 const *bgorder = BGControl::GetBGOrder();
    u16 const *line;
    u16 const *attr;
    u16 xattr;
    u16 xwinx;

    u32 layer = 4;

    do
    {
        switch (bgorder[--layer])
        {
        case 0:
            continue;
            break;
        case 1:
            continue;
            break;
        case 2:
            if (!IsBGEnabled(2)) {continue;}

            m_bg2.BGBitmap::RenderLineMode5();
            m_bg2.BGBitmap::GetLine(line, attr);

            xattr = (m_bg2.GetPriority() | ColorSpecialEffect::GetBGFlags(2)) & 0xFFFF;
            xwinx = DOT_WINDOWUSEBG2;
            break;
        case 3:
            continue;
            break;
        }

        WriteBGFront(line, attr, xattr, xwinx);
    }
    while (layer != 0);
}

void Painter::RenderLine()
{
    u16 const *line;
    u16 const *attr;

    OBJ::RenderLine();
    OBJ::GetWindowLine(m_winx);
    Window::RenderLine(m_winx);    

    u16 bd   = IsForcedBlank() ? 0xFFFF : Palette::GetBackdropColor();
    u16 bdfg = ColorSpecialEffect::GetBackdropFlags() | DOT_LOWESTPRIORITY;

    for (u32 i = 0; i < m_screenwidth; ++i)
    {
        m_attrbuffer[1][i] = DOT_TRANSPARENT | DOT_LOWESTPRIORITY;
        m_linebuffer[0][i] = bd;
        m_attrbuffer[0][i] = bdfg;
    }

    if (!IsForcedBlank())
    {

    // Render BG
    RenderLineBG();


    if (IsOBJEnabled())
    {
        OBJ::GetLine(line, attr);
    
        for (u32 i = 0; i < m_screenwidth; ++i)
        {
            if (((attr[i] & DOT_TRANSPARENT) == DOT_TRANSPARENT) || ((m_winx[i] & DOT_WINDOWUSEOBJ) != DOT_WINDOWUSEOBJ)) {continue;}

            if      ((attr[i] & DOT_PRIORITYBITS) <= (m_attrbuffer[0][i] & DOT_PRIORITYBITS))
            {
                WriteFrontRow(i, line[i], attr[i] | ColorSpecialEffect::GetOBJFlags());
            }
            else if ((attr[i] & DOT_PRIORITYBITS) <= (m_attrbuffer[1][i] & DOT_PRIORITYBITS))
            {
                WriteBackRow(i, line[i], attr[i] | ColorSpecialEffect::GetOBJFlags());
            }
        }
    }

    for (u32 i = 0; i < m_screenwidth; ++i) {if ((m_winx[i] & DOT_WINDOWUSECSE) != DOT_WINDOWUSECSE) {m_attrbuffer[0][i] &= ~DOT_CSEALL;}}
    }

    ColorSpecialEffect::Blend(m_framebuffer[m_VCOUNT.b], m_linebuffer[0], m_attrbuffer[0], m_linebuffer[1], m_attrbuffer[1]);
}

void Painter::SetBGMode(u32 mode)
{
    switch (mode)
    {
    case 0: RenderLineBG = RenderLineBGMode0; break;
    case 1: RenderLineBG = RenderLineBGMode1; break;
    case 2: RenderLineBG = RenderLineBGMode2; break;
    case 3: RenderLineBG = RenderLineBGMode3; break;
    case 4: RenderLineBG = RenderLineBGMode4; break;
    case 5: RenderLineBG = RenderLineBGMode5; break;
    default: RenderLineBG = RenderLineBGMode0; break;
    }
}

s32 GetNextEvent()
{
    return m_ticks;
}

void CompareVCOUNT()
{
    if (m_DISPSTAT.b.b1.b == m_VCOUNT.b)
    {
        m_DISPSTAT.w |= BIT(2);
        if ((m_DISPSTAT.w & BIT(5)) != 0)
        {
            gbaControl::RequestInterrupt(gbaControl::IRQ_VCOUNTER);
        }
    }
    else
    {
        m_DISPSTAT.w &= ~BIT(2);
    }
}

void Sync(s32 ticks)
{
    m_ticks -= ticks;
    while (m_ticks <= 0)
    {
        switch (m_mode)
        {
        case 0:
            m_ticks += m_hblankclk;
            m_DISPSTAT.w |= BIT(1);            
            m_mode = 1;            

            if ((m_DISPSTAT.w & BIT(4)) != 0)
            {
                gbaControl::RequestInterrupt(gbaControl::IRQ_HBLANK);
            }
            if (m_VCOUNT.b < 160)
            {
                gbaDMA::OnHblank();
                Painter::RenderLine();
            }

            break;
        case 1:
            m_ticks += m_lineclk;
            m_DISPSTAT.w &= ~BIT(1);
            m_VCOUNT.b++;
            m_mode = 0;

            if (m_VCOUNT.b >= 2 && m_VCOUNT.b <= 161) {
                gbaDMA::OnCaptureRequest();
            }
            
            if (m_VCOUNT.b == 160)
            {
                gbaDMA::OnVblank();
                if ((m_DISPSTAT.w & BIT(3)) != 0)
                {
                    gbaControl::RequestInterrupt(gbaControl::IRQ_VBLANK);
                }
                m_DISPSTAT.w |= BIT(0);
                Emulator::SendVideoFrame(m_framebuffer);
            }

            else if (m_VCOUNT.b == 162) {
                gbaDMA::OnCaptureEnd();
            }

            else if (m_VCOUNT.b == 227)
            {
                m_DISPSTAT.w &= ~BIT(0);
            }
            else if (m_VCOUNT.b >= 228)
            {
                gbaKeyInput::Sync();
                m_VCOUNT.b = 0;
                m_bg2.OnLeaveVblank();
                m_bg3.OnLeaveVblank();
            }
            CompareVCOUNT();
        }
    }
}

void Reset()
{
    memset(m_PaletteRAM, 0, sizeof(m_PaletteRAM));
    memset(m_VRAM, 0, sizeof(m_VRAM));
    memset(m_OAM, 0, sizeof(m_OAM));
    m_bitmapmode = false;
    m_DISPCNT.w = 0;
    m_u0x04000002.b = 0;
    m_DISPSTAT.w = 0;
    m_VCOUNT.b = 0;
    m_ticks = m_lineclk;
    m_mode = 0;

    BGControl::ResetOrder();
    Painter::SetBGMode(0);
    m_bg0.BGControl::Reset();
    m_bg0.BGText::Reset();
    m_bg1.BGControl::Reset();
    m_bg1.BGText::Reset();
    m_bg2.BGControl::Reset();
    m_bg2.BGText::Reset();
    m_bg2.BGReferencePoint::Reset();
    m_bg2.BGRotationAndScaling::Reset();
    m_bg2.BGBitmap::Reset();
    m_bg3.BGControl::Reset();
    m_bg3.BGText::Reset();
    m_bg3.BGReferencePoint::Reset();
    m_bg3.BGRotationAndScaling::Reset();
    
    OBJ::Reset();
    Window::Reset();
    ColorSpecialEffect::Reset();
}

void WriteDISPCNT_B0(u8 byte)
{
    m_DISPCNT.b.b0.b = byte & ~BIT(3);
    m_bgmode = byte & 7;
    m_bitmapmode = m_bgmode >= 3;
    m_bg2.SetFrame(SUBVAL(m_DISPCNT.w, 4, 1));
    Painter::SetBGMode(m_bgmode);
}

void WriteDISPCNT_B1(u8 byte)
{
    m_DISPCNT.b.b1.b = byte;
}

void Write0x04000002(u8 byte)
{
    m_u0x04000002.b = byte;
    ColorSpecialEffect::SetGreenSwap(BITTEST(byte, 0));
}

void WriteDISPSTAT_B0(u8 byte)
{
    m_DISPSTAT.b.b0.b = (m_DISPSTAT.b.b0.b & 0x7) | (byte & 0xF8);
}

void WriteDISPSTAT_B1(u8 byte)
{
    m_DISPSTAT.b.b1.b = byte;
    CompareVCOUNT();
}

void WriteMOSAIC_B0(u8 byte)
{
    u32 h;
    u32 v;
    m_MOSAIC.b.b0.b = byte;
    h = SUBVAL(byte, 0, 0xF) + 1;
    v = SUBVAL(byte, 4, 0xF) + 1;
    m_bg0.SetMosaic(h, v);
    m_bg1.SetMosaic(h, v);
    m_bg2.BGText::SetMosaic(h, v);
    m_bg3.BGText::SetMosaic(h, v);
    m_bg2.BGRotationAndScaling::SetMosaic(h, v);
    m_bg3.BGRotationAndScaling::SetMosaic(h, v);
}

void WriteMOSAIC_B1(u8 byte)
{
    m_MOSAIC.b.b1.b = byte;
    OBJ::SetMosaic(SUBVAL(byte, 0, 0xF) + 1, SUBVAL(byte, 4, 0xF) + 1);
}

void WritePaletteRAM(u32 address, t32 const *data, gbaMemory::DataType width)
{
    u32 base = address & ~(width - 1) & ~BIT(0) & 0x3FF;
    switch (width)
    {
    case gbaMemory::TYPE_WORD:
        m_PaletteRAM[base | 3] = data->w.w1.b.b1.b;
        m_PaletteRAM[base | 2] = data->w.w1.b.b0.b;
    case gbaMemory::TYPE_HALFWORD:
        m_PaletteRAM[base | 1] = data->w.w0.b.b1.b;
        m_PaletteRAM[base | 0] = data->w.w0.b.b0.b;
        break;
    case gbaMemory::TYPE_BYTE:
        m_PaletteRAM[base | 1] = data->w.w0.b.b0.b;
        m_PaletteRAM[base | 0] = data->w.w0.b.b0.b;
    }
}

void WriteVRAM(u32 address, t32 const *data, gbaMemory::DataType width)
{
    u32 base = address & ((address & 0x10000) != 0 ? 0x17FFF : 0xFFFF) & ~(width - 1);
    switch (width)
    {
    case gbaMemory::TYPE_WORD:
        m_VRAM[base | 3] = data->w.w1.b.b1.b;
        m_VRAM[base | 2] = data->w.w1.b.b0.b;
    case gbaMemory::TYPE_HALFWORD:
        m_VRAM[base | 1] = data->w.w0.b.b1.b;
        m_VRAM[base | 0] = data->w.w0.b.b0.b;
        break;
    case gbaMemory::TYPE_BYTE:
        if (base > (m_bitmapmode ? 0x13FFFU : 0xFFFFU)) {break;}
        base &= ~BIT(0);
        m_VRAM[base | 1] = data->w.w0.b.b0.b;
        m_VRAM[base | 0] = data->w.w0.b.b0.b;
    }
}

void WriteOAM(u32 address, t32 const *data, gbaMemory::DataType width)
{
    u32 base = address & ~(width - 1) & 0x3FF;
    switch (width)
    {
    case gbaMemory::TYPE_WORD:
        m_OAM[base | 3] = data->w.w1.b.b1.b;
        m_OAM[base | 2] = data->w.w1.b.b0.b;
    case gbaMemory::TYPE_HALFWORD:
        m_OAM[base | 1] = data->w.w0.b.b1.b;
    case gbaMemory::TYPE_BYTE:
        m_OAM[base | 0] = data->w.w0.b.b0.b;
    }
}

void WriteIO(u32 address, t32 const *data, gbaMemory::DataType width)
{
    u8 bytes[4] = {data->w.w0.b.b0.b, data->w.w0.b.b1.b, data->w.w1.b.b0.b, data->w.w1.b.b1.b};
    u32 base = address & ~(width - 1);

    for (u32 i = 0; i < (u32)width; i++)
    {
        switch (base + i)
        {
        case 0x04000000: WriteDISPCNT_B0(bytes[i]); break;
        case 0x04000001: WriteDISPCNT_B1(bytes[i]); break;
        case 0x04000002: Write0x04000002(bytes[i]); break;
        case 0x04000004: WriteDISPSTAT_B0(bytes[i]); break;
        case 0x04000005: WriteDISPSTAT_B1(bytes[i]); break;

        case 0x04000008: m_bg0.BGControl::WriteBGCNT_B0(bytes[i]); m_BGXCNT[0].b.b0.b = bytes[i]; m_bg0.BGText::WriteBGCNT_B0(bytes[i]); break;
        case 0x04000009: m_bg0.BGControl::WriteBGCNT_B1(bytes[i]); m_BGXCNT[0].b.b1.b = bytes[i]; m_bg0.BGText::WriteBGCNT_B1(bytes[i]); break;
        case 0x0400000A: m_bg1.BGControl::WriteBGCNT_B0(bytes[i]); m_BGXCNT[1].b.b0.b = bytes[i]; m_bg1.BGText::WriteBGCNT_B0(bytes[i]); break;
        case 0x0400000B: m_bg1.BGControl::WriteBGCNT_B1(bytes[i]); m_BGXCNT[1].b.b1.b = bytes[i]; m_bg1.BGText::WriteBGCNT_B1(bytes[i]);break;

        case 0x0400000C: m_bg2.BGControl::WriteBGCNT_B0(bytes[i]); m_BGXCNT[2].b.b0.b = bytes[i]; m_bg2.BGReferencePoint::WriteBGCNT_B0(bytes[i]); m_bg2.BGRotationAndScaling::WriteBGCNT_B0(bytes[i]); m_bg2.BGText::WriteBGCNT_B0(bytes[i]); break;
        case 0x0400000D: m_bg2.BGControl::WriteBGCNT_B1(bytes[i]); m_BGXCNT[2].b.b1.b = bytes[i]; m_bg2.BGReferencePoint::WriteBGCNT_B1(bytes[i]); m_bg2.BGRotationAndScaling::WriteBGCNT_B1(bytes[i]); m_bg2.BGText::WriteBGCNT_B1(bytes[i]); break;
        case 0x0400000E: m_bg3.BGControl::WriteBGCNT_B0(bytes[i]); m_BGXCNT[3].b.b0.b = bytes[i]; m_bg3.BGReferencePoint::WriteBGCNT_B0(bytes[i]); m_bg3.BGRotationAndScaling::WriteBGCNT_B0(bytes[i]); m_bg3.BGText::WriteBGCNT_B0(bytes[i]); break;
        case 0x0400000F: m_bg3.BGControl::WriteBGCNT_B1(bytes[i]); m_BGXCNT[3].b.b1.b = bytes[i]; m_bg3.BGReferencePoint::WriteBGCNT_B1(bytes[i]); m_bg3.BGRotationAndScaling::WriteBGCNT_B1(bytes[i]); m_bg3.BGText::WriteBGCNT_B1(bytes[i]); break;

        case 0x04000010: m_bg0.WriteBGHOFS_B0(bytes[i]); m_BGXHOFS[0].b.b0.b = bytes[i]; break;
        case 0x04000011: m_bg0.WriteBGHOFS_B1(bytes[i]); m_BGXHOFS[0].b.b1.b = bytes[i]; break;
        case 0x04000012: m_bg0.WriteBGVOFS_B0(bytes[i]); m_BGXVOFS[0].b.b0.b = bytes[i]; break;
        case 0x04000013: m_bg0.WriteBGVOFS_B1(bytes[i]); m_BGXVOFS[0].b.b1.b = bytes[i]; break;
        case 0x04000014: m_bg1.WriteBGHOFS_B0(bytes[i]); m_BGXHOFS[1].b.b0.b = bytes[i]; break;
        case 0x04000015: m_bg1.WriteBGHOFS_B1(bytes[i]); m_BGXHOFS[1].b.b1.b = bytes[i]; break;
        case 0x04000016: m_bg1.WriteBGVOFS_B0(bytes[i]); m_BGXVOFS[1].b.b0.b = bytes[i]; break;
        case 0x04000017: m_bg1.WriteBGVOFS_B1(bytes[i]); m_BGXVOFS[1].b.b1.b = bytes[i]; break;
        case 0x04000018: m_bg2.WriteBGHOFS_B0(bytes[i]); m_BGXHOFS[2].b.b0.b = bytes[i]; break;
        case 0x04000019: m_bg2.WriteBGHOFS_B1(bytes[i]); m_BGXHOFS[2].b.b1.b = bytes[i]; break;
        case 0x0400001A: m_bg2.WriteBGVOFS_B0(bytes[i]); m_BGXVOFS[2].b.b0.b = bytes[i]; break;
        case 0x0400001B: m_bg2.WriteBGVOFS_B1(bytes[i]); m_BGXVOFS[2].b.b1.b = bytes[i]; break;
        case 0x0400001C: m_bg3.WriteBGHOFS_B0(bytes[i]); m_BGXHOFS[3].b.b0.b = bytes[i]; break;
        case 0x0400001D: m_bg3.WriteBGHOFS_B1(bytes[i]); m_BGXHOFS[3].b.b1.b = bytes[i]; break;
        case 0x0400001E: m_bg3.WriteBGVOFS_B0(bytes[i]); m_BGXVOFS[3].b.b0.b = bytes[i]; break;
        case 0x0400001F: m_bg3.WriteBGVOFS_B1(bytes[i]); m_BGXVOFS[3].b.b1.b = bytes[i]; break;

        case 0x04000020: m_bg2.WriteBGPA_B0(bytes[i]); break;
        case 0x04000021: m_bg2.WriteBGPA_B1(bytes[i]); break;
        case 0x04000022: m_bg2.WriteBGPB_B0(bytes[i]); break;
        case 0x04000023: m_bg2.WriteBGPB_B1(bytes[i]); break;
        case 0x04000024: m_bg2.WriteBGPC_B0(bytes[i]); break;
        case 0x04000025: m_bg2.WriteBGPC_B1(bytes[i]); break;
        case 0x04000026: m_bg2.WriteBGPD_B0(bytes[i]); break;
        case 0x04000027: m_bg2.WriteBGPD_B1(bytes[i]); break;
        case 0x04000028: m_bg2.WriteBGX_L_B0(bytes[i]); break;
        case 0x04000029: m_bg2.WriteBGX_L_B1(bytes[i]); break;
        case 0x0400002A: m_bg2.WriteBGX_H_B0(bytes[i]); break;
        case 0x0400002B: m_bg2.WriteBGX_H_B1(bytes[i]); break;
        case 0x0400002C: m_bg2.WriteBGY_L_B0(bytes[i]); break;
        case 0x0400002D: m_bg2.WriteBGY_L_B1(bytes[i]); break;
        case 0x0400002E: m_bg2.WriteBGY_H_B0(bytes[i]); break;
        case 0x0400002F: m_bg2.WriteBGY_H_B1(bytes[i]); break;

        case 0x04000030: m_bg3.WriteBGPA_B0(bytes[i]); break;
        case 0x04000031: m_bg3.WriteBGPA_B1(bytes[i]); break;
        case 0x04000032: m_bg3.WriteBGPB_B0(bytes[i]); break;
        case 0x04000033: m_bg3.WriteBGPB_B1(bytes[i]); break;
        case 0x04000034: m_bg3.WriteBGPC_B0(bytes[i]); break;
        case 0x04000035: m_bg3.WriteBGPC_B1(bytes[i]); break;
        case 0x04000036: m_bg3.WriteBGPD_B0(bytes[i]); break;
        case 0x04000037: m_bg3.WriteBGPD_B1(bytes[i]); break;
        case 0x04000038: m_bg3.WriteBGX_L_B0(bytes[i]); break;
        case 0x04000039: m_bg3.WriteBGX_L_B1(bytes[i]); break;
        case 0x0400003A: m_bg3.WriteBGX_H_B0(bytes[i]); break;
        case 0x0400003B: m_bg3.WriteBGX_H_B1(bytes[i]); break;
        case 0x0400003C: m_bg3.WriteBGY_L_B0(bytes[i]); break;
        case 0x0400003D: m_bg3.WriteBGY_L_B1(bytes[i]); break;
        case 0x0400003E: m_bg3.WriteBGY_H_B0(bytes[i]); break;
        case 0x0400003F: m_bg3.WriteBGY_H_B1(bytes[i]); break;

        case 0x04000040: Window::WriteWIN0H_B0(bytes[i]); break;
        case 0x04000041: Window::WriteWIN0H_B1(bytes[i]); break;
        case 0x04000042: Window::WriteWIN1H_B0(bytes[i]); break;
        case 0x04000043: Window::WriteWIN1H_B1(bytes[i]); break;
        case 0x04000044: Window::WriteWIN0V_B0(bytes[i]); break;
        case 0x04000045: Window::WriteWIN0V_B1(bytes[i]); break;
        case 0x04000046: Window::WriteWIN1V_B0(bytes[i]); break;
        case 0x04000047: Window::WriteWIN1V_B1(bytes[i]); break;

        case 0x04000048: Window::WriteWININ_B0(bytes[i]); m_WININ.b.b0.b = bytes[i]; break;
        case 0x04000049: Window::WriteWININ_B1(bytes[i]); m_WININ.b.b1.b = bytes[i]; break;
        case 0x0400004A: Window::WriteWINOUT_B0(bytes[i]); m_WINOUT.b.b0.b = bytes[i]; break;
        case 0x0400004B: Window::WriteWINOUT_B1(bytes[i]); m_WINOUT.b.b1.b = bytes[i]; break;
        
        case 0x0400004C: WriteMOSAIC_B0(bytes[i]); break;
        case 0x0400004D: WriteMOSAIC_B1(bytes[i]); break;

        case 0x04000050: ColorSpecialEffect::WriteBLDCNT_B0(bytes[i]); m_BLDCNT.b.b0.b = bytes[i]; break;
        case 0x04000051: ColorSpecialEffect::WriteBLDCNT_B1(bytes[i]); m_BLDCNT.b.b1.b = bytes[i]; break;
        case 0x04000052: ColorSpecialEffect::WriteBLDALPHA_B0(bytes[i]); break;
        case 0x04000053: ColorSpecialEffect::WriteBLDALPHA_B1(bytes[i]); break;
        case 0x04000054: ColorSpecialEffect::WriteBLDY(bytes[i]); break;
        }
    }
}

void ReadPaletteRAM(u32 address, t32 *data, gbaMemory::DataType width)
{
    u32 base = address & ~(width - 1) & 0x3FF;
    switch (width)
    {
    case gbaMemory::TYPE_WORD:
        data->w.w1.b.b1.b = m_PaletteRAM[base | 3];
        data->w.w1.b.b0.b = m_PaletteRAM[base | 2];
    case gbaMemory::TYPE_HALFWORD:
        data->w.w0.b.b1.b = m_PaletteRAM[base | 1];
    case gbaMemory::TYPE_BYTE:
        data->w.w0.b.b0.b = m_PaletteRAM[base | 0];
    }
}

void ReadVRAM(u32 address, t32 *data, gbaMemory::DataType width)
{
    u32 base = address & ((address & 0x10000) != 0 ? 0x17FFF : 0xFFFF) & ~(width - 1);
    switch (width)
    {
    case gbaMemory::TYPE_WORD:
        data->w.w1.b.b1.b = m_VRAM[base | 3];
        data->w.w1.b.b0.b = m_VRAM[base | 2];
    case gbaMemory::TYPE_HALFWORD:
        data->w.w0.b.b1.b = m_VRAM[base | 1];
    case gbaMemory::TYPE_BYTE:
        data->w.w0.b.b0.b = m_VRAM[base | 0];
    }
}

void ReadOAM(u32 address, t32 *data, gbaMemory::DataType width)
{
    u32 base = address & ~(width - 1) & 0x3FF;
    switch (width)
    {
    case gbaMemory::TYPE_WORD:
        data->w.w1.b.b1.b = m_OAM[base | 3];
        data->w.w1.b.b0.b = m_OAM[base | 2];
    case gbaMemory::TYPE_HALFWORD:
        data->w.w0.b.b1.b = m_OAM[base | 1];
    case gbaMemory::TYPE_BYTE:
        data->w.w0.b.b0.b = m_OAM[base | 0];
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
        case 0x04000000: *bytes[i] = m_DISPCNT.b.b0.b; break;
        case 0x04000001: *bytes[i] = m_DISPCNT.b.b1.b; break;
        case 0x04000002: *bytes[i] = m_u0x04000002.b; break;
        case 0x04000003: *bytes[i] = 0; break;
        case 0x04000004: *bytes[i] = m_DISPSTAT.b.b0.b; break;
        case 0x04000005: *bytes[i] = m_DISPSTAT.b.b1.b; break;
        case 0x04000006: *bytes[i] = m_VCOUNT.b; break;
        case 0x04000007: *bytes[i] = 0; break;
        case 0x04000008: *bytes[i] = m_BGXCNT[0].b.b0.b; break;
        case 0x04000009: *bytes[i] = m_BGXCNT[0].b.b1.b; break;
        case 0x0400000A: *bytes[i] = m_BGXCNT[1].b.b0.b; break;
        case 0x0400000B: *bytes[i] = m_BGXCNT[1].b.b1.b; break;
        case 0x0400000C: *bytes[i] = m_BGXCNT[2].b.b0.b; break;
        case 0x0400000D: *bytes[i] = m_BGXCNT[2].b.b1.b; break;
        case 0x0400000E: *bytes[i] = m_BGXCNT[3].b.b0.b; break;
        case 0x0400000F: *bytes[i] = m_BGXCNT[3].b.b1.b; break;
        case 0x04000048: *bytes[i] = m_WININ.b.b0.b; break;
        case 0x04000049: *bytes[i] = m_WININ.b.b1.b; break;
        case 0x0400004A: *bytes[i] = m_WINOUT.b.b0.b; break;
        case 0x0400004B: *bytes[i] = m_WINOUT.b.b1.b; break;
        case 0x04000050: *bytes[i] = m_BLDCNT.b.b0.b; break;
        case 0x04000051: *bytes[i] = m_BLDCNT.b.b1.b; break;
        }
    }
}
}
