@echo off

mkdir ..\build
pushd ..\build
cl -Zi -FC ..\code\win32_game.cpp user32.lib gdi32.lib
popd

