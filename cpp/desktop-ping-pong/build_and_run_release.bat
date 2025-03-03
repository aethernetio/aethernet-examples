git submodule update --init --remote aether-client-cpp
cd aether-client-cpp
call git_init.bat
cd ..
mkdir build-example
cd build-example
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
cd Release
ping-pong-example.exe
