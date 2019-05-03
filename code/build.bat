@echo off

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
cl -nologo -MT -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DGAME_INTERNAL=1 -DGAME_SLOW=1 -DGAME_WIN32=1 -Z7 -FC -Fmwin32_game.map ..\code\win32_game.cpp user32.lib gdi32.lib
popd

