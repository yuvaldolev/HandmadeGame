#!/bin/bash

CommonFlags="-DDEBUG -O2 -g -Wall -Werror -Wno-unsequenced -Wno-comment -Wno-multichar -Wno-write-strings -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-unused-result -Wno-strict-aliasing -Wno-int-to-pointer-cast -Wno-switch -Wno-logical-not-parentheses -Wno-return-type -Wno-extern-c-compat -msse4.1 -std=c++11 -fno-rtti -fno-exceptions"
CommonFlags+=" -DGAME_INTERNAL=1 -DGAME_SLOW=1 -DGAME_SDL=1"

# NOTE(yuval): Setup compiler
if [ -n "$(command -v clang++)" ]
then
  CXX=clang++
  CommonFlags+=" -Wno-missing-braces -Wno-null-dereference -Wno-self-assign"
else
  CXX=c++
  CommonFlags+=" -Wno-unused-but-set-variable"
fi

# TODO(yuval): Only darwin is supported for now
SDLFlags="-F /Library/Frameworks -framework SDL2"
PathFlags="-Wl,-rpath,@loader_path"

mkdir -p "../build"
pushd "../build"

$CXX $CommonFlags ../code/game.cpp -fPIC -shared -o game.so
$CXX $CommonFlags ../code/sdl_game.cpp -o sdl_game -ldl $SDLFlags $PathFlags

popd

