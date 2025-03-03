git submodule update --init --remote aether-client-cpp
cd aether-client-cpp
call git_init.bat
cd ..
mkdir build-example
cd build-example
cmake ..
cmake --build .
cd Debug
ping-pong-example.exe
