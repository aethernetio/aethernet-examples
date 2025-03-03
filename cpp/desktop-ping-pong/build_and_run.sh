git submodule update --init --remote aether-client-cpp
cd aether-client-cpp
./git_init.sh
cd ../
mkdir build-example
cd build-example
cmake ..
cmake --build .
./ping-pong-example
