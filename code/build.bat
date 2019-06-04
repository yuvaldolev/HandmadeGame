@echo off

set CommonCompilerFlags=-nologo -MTd -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -Z7 -FC
set CommonCompilerFlags=-DGAME_INTERNAL=1 -DGAME_SLOW=1 -DGAME_WIN32=1 %CommonCompilerFlags%
set CommonLinkerFlags=-opt:ref -incremental:no

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
del *.pdb > NUL 2> NUL

REM 64-bit build
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% -Fmgame.map ..\code\game.cpp -LD /link -PDB:game_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples %CommonLinkerFlags%
del lock.tmp
cl %CommonCompilerFlags% -Fmwin32_game.map ..\code\win32_game.cpp /link %CommonLinkerFlags% user32.lib gdi32.lib Winmm.lib
popd

