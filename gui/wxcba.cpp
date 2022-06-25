//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#define __WXMSW__
#define WXUSINGDLL

#include <wx/wx.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <queue>
#include "directxbackend.h"
#include "../emulator.h"

#define APP_VERSIONSTRING "0.1a"
#define LOG_WIDTH  450
#define LOG_HEIGHT 300

class cbaApp : public wxApp
{
public:
    bool OnInit();
    int OnExit();
    void DeleteObjects();
};

class cbaLogWindow : public wxLogWindow
{
public:
    bool OnFrameClose(wxFrame *frame);
    cbaLogWindow();
};

class cbaMainWindow : public wxFrame
{
DECLARE_EVENT_TABLE();
private:
    void OnMenuFileOpenROM(wxCommandEvent &event);
    void OnMenuFileExit(wxCommandEvent &event);
    void OnMenuViewLog(wxCommandEvent &event);
    void OnMenuMemoryAuto(wxCommandEvent &event);
    void OnMenuMemorySram(wxCommandEvent &event);
    void OnMenuMemoryEeprom(wxCommandEvent &event);
    void OnMenuMemoryFlash64K(wxCommandEvent &event);
    void OnMenuMemoryFlash128K(wxCommandEvent &event);
    void OnClose(wxCloseEvent &event);
    void OnIdle(wxIdleEvent &event);
    void OnKeyDown(wxKeyEvent &event);
    void OnKeyUp(wxKeyEvent &event);
    void OnKillFocus(wxFocusEvent &event);

public:
    cbaMainWindow();
    bool InitDirectX();
};

enum
{
    MAINMENU_FILE_OPENROM,
    MAINMENU_FILE_EXIT,
    MAINMENU_VIEW_LOG,
    MAINMENU_MEMORY_AUTO,
    MAINMENU_MEMORY_SRAM,
    MAINMENU_MEMORY_EEPROM,
    MAINMENU_MEMORY_FLASH64K,
    MAINMENU_MEMORY_FLASH128K
};

BEGIN_EVENT_TABLE(cbaMainWindow, wxFrame)
    EVT_MENU(MAINMENU_FILE_OPENROM, cbaMainWindow::OnMenuFileOpenROM)
    EVT_MENU(MAINMENU_FILE_EXIT,    cbaMainWindow::OnMenuFileExit)
    EVT_MENU(MAINMENU_VIEW_LOG,     cbaMainWindow::OnMenuViewLog)
    EVT_MENU(MAINMENU_MEMORY_AUTO,  cbaMainWindow::OnMenuMemoryAuto)
    EVT_MENU(MAINMENU_MEMORY_SRAM,  cbaMainWindow::OnMenuMemorySram)
    EVT_MENU(MAINMENU_MEMORY_EEPROM, cbaMainWindow::OnMenuMemoryEeprom)
    EVT_MENU(MAINMENU_MEMORY_FLASH64K, cbaMainWindow::OnMenuMemoryFlash64K)
    EVT_MENU(MAINMENU_MEMORY_FLASH128K, cbaMainWindow::OnMenuMemoryFlash128K)
    EVT_CLOSE(cbaMainWindow::OnClose)
    EVT_IDLE(cbaMainWindow::OnIdle)
    EVT_KEY_DOWN(cbaMainWindow::OnKeyDown)
    EVT_KEY_UP(cbaMainWindow::OnKeyUp)
    EVT_KILL_FOCUS(cbaMainWindow::OnKillFocus)
END_EVENT_TABLE()

IMPLEMENT_APP(cbaApp);

cbaLogWindow        *LogWnd;
cbaMainWindow       *MainWnd;
HANDLE               hEmulator;
DWORD                EmulatorThreadId;
wxMutex             *LogMutex;
std::queue<wxString> LogFIFO;
volatile u16         KeypadState;

gbaCartridge::BackupType Battery;

namespace Emulator
{
    void LogMessage(const char* format, ...)
    {
        LogMutex->Lock();
        va_list list;
        va_start(list, format);
        size_t szbuf = _vscprintf(format, list) + 1;
        char* str = new char[szbuf];
        vsprintf_s(str, szbuf, format, list);
        va_end(list);
        wxString f(str, wxConvUTF8);
        delete[] str;
        LogFIFO.push(f);
        LogMutex->Unlock();
    }
}

cbaLogWindow::cbaLogWindow() : wxLogWindow(MainWnd, wxT("Mensajes"))
{
    MainWnd->GetMenuBar()->Check(MAINMENU_VIEW_LOG, true);

    wxMenuBar *menu = GetFrame()->GetMenuBar();
    GetFrame()->SetMenuBar(0);
    delete menu;

    wxStatusBar *status = GetFrame()->GetStatusBar();
    GetFrame()->SetStatusBar(0);
    delete status;

    GetFrame()->SetSize(LOG_WIDTH, LOG_HEIGHT);
    PassMessages(false);
}

bool cbaLogWindow::OnFrameClose(wxFrame *WXUNUSED(frame))
{
    MainWnd->GetMenuBar()->Check(MAINMENU_VIEW_LOG, false);
    Show(false);
    return false;
}

void EmptyLogFIFO()
{
    LogMutex->Lock();
    while (!LogFIFO.empty())
    {
        wxLogMessage(LogFIFO.front());
        LogFIFO.pop();
    }
    LogMutex->Unlock();
}

void CloseEmulatorThread()
{
    if (hEmulator != 0)
    {
        Emulator::LogMessage("Esperando a que el hilo de ejecucion del emulador termine...");
        gbaCore::StopEmulation();
        WaitForSingleObject(hEmulator, INFINITE);
        DWORD exitcode;
        GetExitCodeThread(hEmulator, &exitcode);
        Emulator::LogMessage("El hilo de ejecucion del emulador (0x%X) ha finalizado con codigo %d (0x%X)\n", EmulatorThreadId, exitcode, exitcode);
        CloseHandle(hEmulator);
        hEmulator = 0;
    }
}

DWORD WINAPI StartEmulation(void *filename)
{
    gbaCore::StartEmulation();
    Emulator::LogMessage("El emulador se ha detenido");
    ResetFrame();
    StopSound();
    ResetSoundBuffer();
    return 0;
}

void PrepareEmulator(wxString &romfilename)
{
    CloseEmulatorThread();
    gbaCartridge::Load(romfilename.mb_str(wxConvUTF8), Battery, true);
    hEmulator = CreateThread(0, 0, StartEmulation, 0, CREATE_SUSPENDED, &EmulatorThreadId);
    if (hEmulator == 0)
    {
        Emulator::LogMessage("Error al crear el hilo de ejecucion del emulador");
    }
    else
    {
        Emulator::LogMessage("Se ha creado el hilo de ejecucion del emulador (0x%X)", EmulatorThreadId);
        ResumeThread(hEmulator);
    }
}

void PressKeypadButton(gbaKeyInput::KeypadButton button)
{
    _InterlockedAnd16((short *)&KeypadState, gbaKeyInput::BUTTON_ALL & ~button);
}

void ReleaseKeypadButton(gbaKeyInput::KeypadButton button)
{
    _InterlockedOr16((short *)&KeypadState, button);
}

void ResetKeypad()
{
    ReleaseKeypadButton(gbaKeyInput::BUTTON_ALL);
}

cbaMainWindow::cbaMainWindow() : wxFrame(0, wxID_ANY, wxT("Heron v") wxT(APP_VERSIONSTRING), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE & ~wxMINIMIZE_BOX)
{
    wxMenuBar *menu_main = new wxMenuBar();
    wxMenu    *menu_file = new wxMenu();
    wxMenu    *menu_view = new wxMenu();

    menu_file->Append(MAINMENU_FILE_OPENROM, wxT("Abrir &ROM\tCTRL+O"));
    menu_file->AppendSeparator();
    menu_file->Append(MAINMENU_FILE_EXIT,    wxT("&Salir\tALT+F4"));

    menu_view->Append(MAINMENU_VIEW_LOG,     wxT("&Mensajes\tCTRL+L"), wxEmptyString, true);

    wxMenu *menu_memory = new wxMenu();
    menu_memory->Append(MAINMENU_MEMORY_AUTO,     wxT("Deteccion Automatica"), wxEmptyString, true);
    menu_memory->AppendSeparator();
    menu_memory->Append(MAINMENU_MEMORY_SRAM,      wxT("SRAM 32KB"),           wxEmptyString, true);
    menu_memory->Append(MAINMENU_MEMORY_EEPROM,    wxT("EEPROM 512B/8KB"),     wxEmptyString, true);
    menu_memory->Append(MAINMENU_MEMORY_FLASH64K,  wxT("FLASH ROM 64KB"),      wxEmptyString, true);
    menu_memory->Append(MAINMENU_MEMORY_FLASH128K, wxT("FLASH ROM 128KB"),     wxEmptyString, true);

    Battery = gbaCartridge::BACKUP_NOID;

    menu_main->Append(menu_file,   wxT("&Archivo"));
    menu_main->Append(menu_view,   wxT("&Ver"));
    menu_main->Append(menu_memory, wxT("&Memoria"));

    menu_main->Check(MAINMENU_MEMORY_AUTO, true);

    SetMenuBar(menu_main);
    SetExtraStyle(wxWS_EX_PROCESS_IDLE);
    SetClientSize(GBA_SCREENWIDTH, GBA_SCREENHEIGHT);
    SetMinSize(GetSize());
}

void cbaMainWindow::OnMenuFileOpenROM(wxCommandEvent &WXUNUSED(event))
{
    wxString romfilename = wxFileSelector(wxT("Abrir ROM"),
                                          wxEmptyString,
                                          wxEmptyString,
                                          wxEmptyString,
                                          wxT("GBA ROMs (*.gba;*.agb;*.bin)|*.gba;*.agb;*.bin|Todos los archivos (*.*)|*.*"),
                                          wxFD_OPEN | wxFD_FILE_MUST_EXIST,
                                          this);
    if (!romfilename.IsEmpty()) {PrepareEmulator(romfilename);}
}

void cbaMainWindow::OnMenuFileExit(wxCommandEvent &WXUNUSED(event))
{
    Close(true);
}

void cbaMainWindow::OnMenuViewLog(wxCommandEvent &WXUNUSED(event))
{
    LogWnd->Show(GetMenuBar()->IsChecked(MAINMENU_VIEW_LOG));
}

void cbaMainWindow::OnMenuMemoryAuto(wxCommandEvent &event) {
    Battery = gbaCartridge::BACKUP_NOID;

    GetMenuBar()->Check(MAINMENU_MEMORY_AUTO, true);
    GetMenuBar()->Check(MAINMENU_MEMORY_SRAM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_EEPROM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH64K, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH128K, false);
}

void cbaMainWindow::OnMenuMemorySram(wxCommandEvent &event) {
    Battery = gbaCartridge::BACKUP_SRAM;

    GetMenuBar()->Check(MAINMENU_MEMORY_AUTO, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_SRAM, true);
    GetMenuBar()->Check(MAINMENU_MEMORY_EEPROM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH64K, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH128K, false);
}

void cbaMainWindow::OnMenuMemoryEeprom(wxCommandEvent &event) {
    Battery = gbaCartridge::BACKUP_EEPROM;

    GetMenuBar()->Check(MAINMENU_MEMORY_AUTO, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_SRAM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_EEPROM, true);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH64K, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH128K, false);
}

void cbaMainWindow::OnMenuMemoryFlash64K(wxCommandEvent &event) {
    Battery = gbaCartridge::BACKUP_FLASH512;

    GetMenuBar()->Check(MAINMENU_MEMORY_AUTO, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_SRAM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_EEPROM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH64K, true);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH128K, false);
}

void cbaMainWindow::OnMenuMemoryFlash128K(wxCommandEvent &event) {
    Battery = gbaCartridge::BACKUP_FLASH1M;

    GetMenuBar()->Check(MAINMENU_MEMORY_AUTO, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_SRAM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_EEPROM, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH64K, false);
    GetMenuBar()->Check(MAINMENU_MEMORY_FLASH128K, true);
}

void cbaMainWindow::OnClose(wxCloseEvent &event)
{
    delete LogWnd;
    CloseEmulatorThread();
    ReleaseDirect3D();
    ReleaseDirectSound();
    event.Skip();
}

void cbaMainWindow::OnIdle(wxIdleEvent &event)
{
    EmptyLogFIFO();
    event.Skip();
}

void cbaMainWindow::OnKeyDown(wxKeyEvent &event)
{
    switch (event.GetKeyCode())
    {
    case 'X':        PressKeypadButton(gbaKeyInput::BUTTON_A);      break;
    case 'C':        PressKeypadButton(gbaKeyInput::BUTTON_B);      break;
    case WXK_SPACE:  PressKeypadButton(gbaKeyInput::BUTTON_SELECT); break;
    case WXK_RETURN: PressKeypadButton(gbaKeyInput::BUTTON_START);  break;
    case WXK_RIGHT:  PressKeypadButton(gbaKeyInput::BUTTON_RIGHT);  break;
    case WXK_LEFT:   PressKeypadButton(gbaKeyInput::BUTTON_LEFT);   break;
    case WXK_UP:     PressKeypadButton(gbaKeyInput::BUTTON_UP);     break;
    case WXK_DOWN:   PressKeypadButton(gbaKeyInput::BUTTON_DOWN);   break;
    case 'D':        PressKeypadButton(gbaKeyInput::BUTTON_R);      break;
    case 'S':        PressKeypadButton(gbaKeyInput::BUTTON_L);      break;
    }

    event.Skip();
}

void cbaMainWindow::OnKeyUp(wxKeyEvent &event)
{
    switch (event.GetKeyCode())
    {
    case 'X':        ReleaseKeypadButton(gbaKeyInput::BUTTON_A);      break;
    case 'C':        ReleaseKeypadButton(gbaKeyInput::BUTTON_B);      break;
    case WXK_SPACE:  ReleaseKeypadButton(gbaKeyInput::BUTTON_SELECT); break;
    case WXK_RETURN: ReleaseKeypadButton(gbaKeyInput::BUTTON_START);  break;
    case WXK_RIGHT:  ReleaseKeypadButton(gbaKeyInput::BUTTON_RIGHT);  break;
    case WXK_LEFT:   ReleaseKeypadButton(gbaKeyInput::BUTTON_LEFT);   break;
    case WXK_UP:     ReleaseKeypadButton(gbaKeyInput::BUTTON_UP);     break;
    case WXK_DOWN:   ReleaseKeypadButton(gbaKeyInput::BUTTON_DOWN);   break;
    case 'D':        ReleaseKeypadButton(gbaKeyInput::BUTTON_R);      break;
    case 'S':        ReleaseKeypadButton(gbaKeyInput::BUTTON_L);      break;
    }

    event.Skip();
}

void cbaMainWindow::OnKillFocus(wxFocusEvent &event)
{
    ResetKeypad();
    event.Skip();
}

bool cbaMainWindow::InitDirectX()
{
    HWND hWnd = (HWND)GetHWND();
    if (!InitializeDirect3D(hWnd, true))    {return false;}
    if (!InitializeDirectSound(hWnd, true)) {return false;}
    ResetFrame();
    ResetSoundBuffer();
    return true;
}

namespace Emulator
{
u16 ReadKeypad()
{
    return KeypadState;
}
}

void cbaApp::DeleteObjects()
{
    delete LogMutex;
}

bool cbaApp::OnInit()
{
    wxString biosfilename;

    LogMutex = new wxMutex();
    MainWnd  = new cbaMainWindow();

    SetTopWindow(MainWnd);

    LogWnd = new cbaLogWindow();
    MainWnd->Show();
    
    if (!MainWnd->InitDirectX())
    {
        wxMessageBox(wxT("Error al inicializar DirectX"), wxT("Error"), wxOK | wxICON_ERROR);
        goto _INITFAIL;
    }

    Emulator::LogMessage("Project Heron - Emulador de Game Boy Advance");
    Emulator::LogMessage("jcds (jdibenes@outlook.com)");
    Emulator::LogMessage("Version " APP_VERSIONSTRING " [" __DATE__ " " __TIME__ "]\n");

    biosfilename = wxPathOnly(wxStandardPaths::Get().GetExecutablePath()) + wxT("\\") + wxT("gba_bios.bin");

    if (!gbaBIOS::Load(biosfilename.mb_str(wxConvUTF8)))
    {
        wxMessageBox(wxT("Este programa requiere una imagen del BIOS del GBA.\n")
                     wxT("El archivo debe llamarse gba_bios.bin, ")
                     wxT("tener un tamaño de 16KB ")
                     wxT("y estar ubicado en la misma carpeta que este programa.\n") + biosfilename,
                     wxT("Error"), wxOK | wxICON_ERROR);
        goto _INITFAIL;
    }

    ResetKeypad();
    return true;

_INITFAIL:
    DeleteObjects();
    return false;
}

int cbaApp::OnExit()
{
    DeleteObjects();
    return wxApp::OnExit();
}
