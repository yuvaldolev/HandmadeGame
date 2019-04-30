@echo off

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
cl -DGAME_INTERNAL=1 -DGAME_SLOW=1 -DGAME_WIN32=1 -Zi -FC ..\code\win32_game.cpp user32.lib gdi32.lib
popd

