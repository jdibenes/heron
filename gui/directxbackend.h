//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

bool InitializeDirect3D(HWND hWnd, bool vsync);
void ResetFrame();
void ReleaseDirect3D();
bool InitializeDirectSound(HWND hWnd, bool global);
void StopSound();
void ResetSoundBuffer();
void ReleaseDirectSound();
