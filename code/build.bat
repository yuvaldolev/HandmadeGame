@echo off

mkdir ..\build
pushd ..\build
cl -Zi ..\code\Win32Entry.cpp user32.lib gdi32.lib
popd

