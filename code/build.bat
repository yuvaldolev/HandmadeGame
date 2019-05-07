@echo off

set CommonCompilerFlags=-nologo -MT -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -Z7 -FC
set CommonCompilerFlags=-DGAME_INTERNAL=1 -DGAME_SLOW=1 -DGAME_WIN32=1 %CommonCompilerFlags%
set CommonLinkerFlags=-opt:ref user32.lib gdi32.lib Winmm.lib

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
cl %CommonCompilerFlags% -Fmwin32_game.map ..\code\win32_game.cpp /link %CommonLinkerFlags%
popd

