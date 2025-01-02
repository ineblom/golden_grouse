#!/bin/sh

echo Building...

glfw_dir=$(brew --prefix glfw)
ffmpeg_dir=$(brew --prefix ffmpeg)

includes="-I ./deps/imgui/ -I ./deps/glad/include -I $glfw_dir/include -I $ffmpeg_dir/include"
libs="-L ./ -L $glfw_dir/lib -L $ffmpeg_dir/lib -lglfw -limgui -lglad -lpthread -lavcodec -lavformat -lswscale"
frameworks="-framework OpenGL"
warnings="-Wno-unused-function"

clang++ -Wall $warnings -std=c++17 ./src/main.cpp $includes $frameworks $libs -o ./golden_grouse

if [ $? == 0 ]
then
  echo ✅ Complie Successful ✅

  if [ "$1" == "RUN" ]
  then
    ./golden_grouse
  fi
else
  echo ❌ Complie Failed ❌
fi
