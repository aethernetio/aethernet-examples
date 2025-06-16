git submodule update --init --remote aether-client-cpp
mkdir build-example
cmake -G "Visual Studio 17 2022" -A Win32 -B build-example
cmake --build build-example --config Release
cd build-example\Release
ping-pong-example.exe
