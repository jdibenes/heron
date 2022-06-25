//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#include <d3d9.h>
#include "../emulator.h"

#define D3DFVF_TLVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

struct D3DTLVERTEX
{
    float x;
    float y;
    float z;
    float rhw;
    D3DCOLOR color;
    float u;
    float v;
};

IDirect3D9             *pD3D9;
IDirect3DDevice9       *pDevice;
IDirect3DVertexBuffer9 *pVertexBuffer;
IDirect3DTexture9      *pTexture;
D3DPRESENT_PARAMETERS   D3Dpp;
D3DDISPLAYMODE          DisplayMode;

bool InitializeDevice(HWND hWnd, bool vsync)
{
    HRESULT hrval;

    ZeroMemory(&D3Dpp, sizeof(D3Dpp));

    D3Dpp.SwapEffect           = D3DSWAPEFFECT_DISCARD;
    D3Dpp.BackBufferCount      = 1;
    D3Dpp.BackBufferWidth      = GBA_SCREENWIDTH;
    D3Dpp.BackBufferHeight     = GBA_SCREENHEIGHT;
    D3Dpp.BackBufferFormat     = DisplayMode.Format;
    D3Dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    D3Dpp.Windowed             = TRUE;

    hrval = pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &D3Dpp, &pDevice);
    if (hrval == D3D_OK) {return true;}
    hrval = pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &D3Dpp, &pDevice);
    if (hrval == D3D_OK) {return true;}
    hrval = pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &D3Dpp, &pDevice);
    return hrval == D3D_OK;
}

bool InitializeVertexBuffer()
{
    D3DTLVERTEX *vertices;
    HRESULT      hrval;

    pDevice->SetVertexShader(0);
    pDevice->SetFVF(D3DFVF_TLVERTEX);

    hrval = pDevice->CreateVertexBuffer(sizeof(D3DTLVERTEX) * 4, 0, D3DFVF_TLVERTEX, D3DPOOL_DEFAULT, &pVertexBuffer, 0);
    if (hrval != D3D_OK) {return false;}

    pVertexBuffer->Lock(0, 0, (void **)&vertices, 0);

    vertices[0].x =   0.0f; vertices[0].y = 256.0f; vertices[0].z = 0.0f; vertices[0].rhw = 1.0f; vertices[0].color = 0xFFFFFFFF; vertices[0].u = 0.0f; vertices[0].v = 1.0f;    
    vertices[1].x =   0.0f; vertices[1].y =   0.0f; vertices[1].z = 0.0f; vertices[1].rhw = 1.0f; vertices[1].color = 0xFFFFFFFF; vertices[1].u = 0.0f; vertices[1].v = 0.0f;
    vertices[2].x = 256.0f; vertices[2].y = 256.0f; vertices[2].z = 0.0f; vertices[2].rhw = 1.0f; vertices[2].color = 0xFFFFFFFF; vertices[2].u = 1.0f; vertices[2].v = 1.0f;
    vertices[3].x = 256.0f; vertices[3].y =   0.0f; vertices[3].z = 0.0f; vertices[3].rhw = 1.0f; vertices[3].color = 0xFFFFFFFF; vertices[3].u = 1.0f; vertices[3].v = 0.0f;
    
    pVertexBuffer->Unlock();

    pDevice->SetStreamSource(0, pVertexBuffer, 0, sizeof(D3DTLVERTEX));
    return true;
}

bool InitializeTexture()
{
    HRESULT hrval = pDevice->CreateTexture(256, 256, 0, D3DUSAGE_DYNAMIC, DisplayMode.Format, D3DPOOL_DEFAULT, &pTexture, 0);
    if (hrval != D3D_OK) {return false;}
    
    
    pDevice->SetTexture(0, pTexture);
    pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_NONE);
    pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_NONE);
    pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    return true;
}

bool InitializeDirect3D(HWND hWnd, bool vsync)
{
    pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D9) {return false;}

    pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &DisplayMode);
    if ((DisplayMode.Format != D3DFMT_A8R8G8B8) && (DisplayMode.Format != D3DFMT_X8R8G8B8)) {return false;}

    if (!InitializeDevice(hWnd, vsync)) {return false;}
    if (!InitializeVertexBuffer())      {return false;}
    if (!InitializeTexture())           {return false;}

    return true;
}

void ReleaseDirect3D()
{
    if (pTexture)      {pDevice->SetTexture(0, 0);            pTexture     ->Release(); pTexture      = 0;}
    if (pVertexBuffer) {pDevice->SetStreamSource(0, 0, 0, 0); pVertexBuffer->Release(); pVertexBuffer = 0;}
    if (pDevice)       {                                      pDevice      ->Release(); pDevice       = 0;}
    if (pD3D9)         {                                      pD3D9        ->Release(); pD3D9         = 0;}
}

void ResetFrame()
{
    pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
    pDevice->Present(0, 0, 0, 0);
}

u32 ConvertColor16to32(u16 color)
{
    return 0xFF000000 | ((color & 0x001F) << 19) | ((color & 0x03E0) <<  6) | ((color & 0x7C00) >>  7);
}

namespace Emulator
{
void SendVideoFrame(u16 frame[GBA_SCREENHEIGHT][GBA_SCREENWIDTH])
{
    D3DLOCKED_RECT lockedrect;
    u32 vpos;
    
    pTexture->LockRect(0, &lockedrect, 0, 0);

    for (u32 line = 0; line < GBA_SCREENHEIGHT; line++)
    {
        vpos = line * 256;
        for (u32 dot = 0; dot < GBA_SCREENWIDTH; dot++) {((u32 *)lockedrect.pBits)[vpos + dot] = ConvertColor16to32(frame[line][dot]);}
    }

    pTexture->UnlockRect(0);

    pDevice->BeginScene();
    pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    pDevice->EndScene();
    pDevice->Present(0, 0, 0, 0);
}
}
