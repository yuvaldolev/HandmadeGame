@echo off

mkdir ..\build
pushd ..\build
cl -Zi ..\code\win32_entry.cpp
popd
