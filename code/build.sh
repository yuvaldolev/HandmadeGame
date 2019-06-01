#!/bin/bash

CommonFlags="-DDEBUG -g -Weverything -Wall -Werror -fdiagnostics-absolute-paths -std=c++11 -fno-rtti -fno-exceptions"
CommonFlags+=" -Wno-unsequenced -Wno-comment -Wno-unused-variable -Wno-unused-function -Wno-unused-result -Wno-switch -Wno-old-style-cast -Wno-zero-as-null-pointer-constant -Wno-string-conversion  -Wno-newline-eof -Wno-c++98-compat-pedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-padded -Wno-missing-prototypes -Wno-cast-align -Wno-sign-conversion -Wno-switch-enum -Wno-double-promotion -Wno-gnu-zero-variadic-macro-arguments"
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

#echo "Compiling Using: $CXX"
#echo

# Mac Build
CommonFlags+=" -DGAME_MAC=1"
MacFlags="-framework Cocoa -framework OpenGL -framework AudioToolbox -framework IOKit"
$CXX $CommonFlags ../code/game.cpp -fPIC -shared -o game.dylib
$CXX $CommonFlags -Wno-deprecated-declarations ../code/mac_game.mm -o mac_game -ldl $MacFlags $PathFlags
popd

