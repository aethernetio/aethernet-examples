## Build Instructions
### Init dependencies
It uses git submodules to manage dependencies.
```
git submodule update --remote ./aether-client-cpp
```

### For desktop
For desktop a regular cmake project is used.
Make build directory, cd to it and configure cmake.
```sh
mkdir build
cd build
cmake ..
cmake --build . --parallel
```

### ESP IDF
For ESP IDF the same CMakeLists.txt is used.
Make build directory, cd to it and configure cmake.
```sh
mkdir build
cd build

idf.py -C .. -B . set-target <write your board here>
idf.py -C .. -B . build
idf.py -C .. -B . flash
```

### PlatformIO
For PlatformIO the platformio.ini is provided. Edit it to match your board.
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
