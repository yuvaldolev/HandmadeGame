@echo off

mkdir ..\build
pushd ..\build
cl /FeHandmadeGame -Zi -FC ..\code\Win32Entry.cpp user32.lib gdi32.lib
popd

