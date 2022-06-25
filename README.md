# About

This is a Game Boy Advance emulator I wrote around 2013. Requires a GBA bios copy (not provided) to run.

## References

GBATEK: https://www.problemkaputt.de/gbatek.htm

Pan Docs: https://gbdev.io/pandocs/

GBSOUND.TXT

Game Boy Sound Operation.txt

ARM Architecture Reference Manual

ARM7TDMI Data Sheet

ARM7TDMI Technical Reference Manual 

## Build Instructions (Visual Studio)

Note: Requires wxWidgets (https://www.wxwidgets.org/)

Set Character Set to Use Unicode Character Set.

Disable SDL checks.

Include wxWidgets headers.

Include the following libraries: d3d9.lib, dsound.lib, dinput8.lib, dxguid.lib, wxWidgets libs.

Set SubSystem as Windows.

Build.
