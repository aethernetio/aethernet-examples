## Build Instructions
### Init dependencies
The CMake projects fetch the exact published SDK revision recorded in
`src/CMakeLists.txt` through CPM. Local SDK development can use
`-DCPM_aether-client-cpp_SOURCE=/absolute/path/to/aether-client-cpp`.

### For desktop
For desktop a regular cmake project is used.
Make build directory, cd to it and configure cmake.
```sh
cmake -S . -B build -DSMART_HOME_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The offline test freezes the original Java P2P wire protocol, including request
IDs, typed payloads, result/error replies and deferred device completion. See
[fixture provenance](tests/README.md) and the repository's
[validation matrix](../../docs/sdk-compatibility.md) for verification limits.
The SDK's push API is adapted locally; Java method IDs and DTO fields stay the
same. Device operations remain asynchronous.

### ESP IDF
For ESP IDF the same CMakeLists.txt is used.
Make build directory, cd to it and configure cmake.
ESP is using wifi connection for internet. Pass your ssid and password to cmake.
```sh
mkdir build
cd build

idf.py -C .. -B . set-target <write your board here>
idf.py -C .. -B . build -DWIFI_SSID=<your_ssid> -DWIFI_PASSWORD=<your_password>
idf.py -C .. -B . flash
```

### PlatformIO
For PlatformIO the platformio.ini is provided.
ESP is using wifi connection for internet.
Edit platformio.ini to match your board and set ssid and password.
```sh
pio run -e <env_name> -t upload
```
## Running
Open application logs and wait till client is ready.
Don't miss the big message about it.
```
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 REGISTERED CLIENT'S UID: b9dc68c0-e3ea-47e6-8980-3d4b6ee8611f
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
```

Copy the UID from the logs and paste it into Aether [Smart Home page.](https://github.com/aethernetio/client-java/tree/main/smarthome)
