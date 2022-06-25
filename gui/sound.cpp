//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#define DIRECTSOUND_VERSION 0x0800
#include <dsound.h>
#include "../emulator.h"

#define BUFFER_SIZE  GBA_SAMPLERATE / 2

LPDIRECTSOUND8       pDS8;
LPDIRECTSOUNDBUFFER  pDSB_SO;
LPDIRECTSOUNDBUFFER8 pSB8_SO8;
s16                  SoundBuffer[BUFFER_SIZE];
s32                  BufferPos;

void PlayNextSoundFrame()
{
    BYTE *p_data1;
    BYTE *p_data2;
    DWORD p_size1;
    DWORD p_size2;

    pSB8_SO8->Stop();
    pSB8_SO8->Lock(0, sizeof(SoundBuffer), (LPVOID *)&p_data1, &p_size1, (LPVOID *)&p_data2, &p_size2, DSBLOCK_ENTIREBUFFER);
    memcpy(p_data1, SoundBuffer, sizeof(SoundBuffer));
    pSB8_SO8->Unlock((LPVOID)p_data1, p_size1, (LPVOID)p_data2, p_size2);

    pSB8_SO8->SetCurrentPosition(0);
    pSB8_SO8->Play(0, 0, 0);
}

bool InitializeDirectSound(HWND hWnd, bool global)
{
    HRESULT      hrval;
    DSBUFFERDESC dsbd;
    WAVEFORMATEX wfex;

    hrval = DirectSoundCreate8(&DSDEVID_DefaultPlayback, &pDS8, 0);
    if (hrval != DS_OK) {return false;}

    hrval = pDS8->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
    if (hrval != DS_OK) {return false;}

    wfex.wFormatTag      = WAVE_FORMAT_PCM;
    wfex.nChannels       = 2;
    wfex.nSamplesPerSec  = GBA_SAMPLERATE;
    wfex.nAvgBytesPerSec = GBA_SAMPLERATE * 2 * 2;
    wfex.nBlockAlign     = 4;
    wfex.wBitsPerSample  = 16;
    wfex.cbSize          = 0;

    dsbd.dwSize          = sizeof(DSBUFFERDESC);
    dsbd.dwFlags         = global ? DSBCAPS_GLOBALFOCUS : 0;
    dsbd.dwBufferBytes   = sizeof(SoundBuffer);
    dsbd.dwReserved      = 0;
    dsbd.lpwfxFormat     = &wfex;
    dsbd.guid3DAlgorithm = DS3DALG_DEFAULT;

    hrval = pDS8->CreateSoundBuffer(&dsbd, &pDSB_SO, 0);
    if (hrval != DS_OK) {return false;}

    hrval = pDSB_SO->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID *)&pSB8_SO8);
    return hrval == DS_OK;
}

void StopSound()
{
    pSB8_SO8->Stop();
}

void ResetSoundBuffer()
{
    memset(SoundBuffer, 0, sizeof(SoundBuffer));
    BufferPos = 0;
}

void ReleaseDirectSound()
{
    if (pSB8_SO8) {pSB8_SO8->Stop(); pSB8_SO8->Release(); pSB8_SO8 = 0;}
    if (pDSB_SO)  {                  pDSB_SO ->Release(); pDSB_SO  = 0;}
    if (pDS8)     {                  pDS8    ->Release(); pDS8     = 0;}
}

namespace Emulator
{
void SendSoundSample(s32 so1, s32 so2)
{
    SoundBuffer[BufferPos]     = (s16)((so1 * 32));
    SoundBuffer[BufferPos + 1] = (s16)((so2 * 32));
    BufferPos += 2;
    if (BufferPos < BUFFER_SIZE) {return;}
    BufferPos = 0;
    PlayNextSoundFrame();
}
}
