#!/bin/bash

CommonFlags="-DDEBUG -g -Wall -Werror -Wno-unsequenced -Wno-comment -Wno-multichar -Wno-write-strings -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-unused-result -Wno-strict-aliasing -Wno-int-to-pointer-cast -Wno-switch -Wno-logical-not-parentheses -Wno-return-type -Wno-extern-c-compat -msse4.1 -std=c++11 -fno-rtti -fno-exceptions"
CommonFlags+=" -DGAME_INTERNAL=1 -DGAME_SLOW=1"

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
PathFlags="-Wl,-rpath,@loader_path"

mkdir -p "../build"
pushd "../build"

# SDL Build
# CommonFlags+=" -DGAME_SDL=1"
# SDLFlags="-F /Library/Frameworks -framework SDL2"
# $CXX $CommonFlags ../code/game.cpp -fPIC -shared -o game.so
# $CXX $CommonFlags ../code/sdl_game.cpp -o sdl_game -ldl $SDLFlags $PathFlags

# Mac Build
CommonFlags+=" -DGAME_MAC=1"
MacFlags="-framework Cocoa"
$CXX $CommonFlags ../code/game.cpp -fPIC -shared -o game.dylib
$CXX $CommonFlags ../code/mac_game.mm -o mac_game -ldl $MacFlags $PathFlags
popd

