@echo off

mkdir ..\build
pushd ..\build
cl -Zi /FeHandmadeGame ..\code\Win32Entry.cpp user32.lib gdi32.lib
popd

