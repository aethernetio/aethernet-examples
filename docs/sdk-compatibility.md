# SDK compatibility checks

The modern desktop projects and C echo, C++ echo and SmartHome ESP-IDF projects
pin Aether SDK `59153adf1fb8baba088b28b50051fb961e8d6eaf`. Its dependency set
follows Tele `main` and pins Objects
`80fbb4c33bebd660c9bdf0687311cee30de74f39`, misc `6839624` and numeric `9d79bd7`.
Telemetry is intentionally a moving branch; validation records its resolved SHA
alongside the immutable SDK revision.

The Tele-main follow-up resolved Tele to
`65eda03928030b8cc04432e2f98ba4e3242b6f27`. All four desktop Release builds below
passed again, including the SmartHome CTest (1/1, 0.31 seconds), and all three
ESP32-C6 firmware builds passed again with the sizes below unchanged.

Local development can override the SDK with
`-DCPM_aether-client-cpp_SOURCE=/absolute/path/to/aether-client-cpp`.
Release verification records actual remote Git HEADs, including the resolved
Telemetry main head, with no local source overrides.

Build each desktop project in a separate directory:

```sh
cmake -S cpp/desktop-ping-pong -B build-ping-pong
cmake --build build-ping-pong --config Release --target ping-pong-example
cmake -S cpp/echo_cpp -B build-echo-cpp
cmake --build build-echo-cpp --config Release --target echo-cpp
cmake -S cpp/echo_capi -B build-echo-capi
cmake --build build-echo-capi --config Release --target echo-capi
cmake -S cpp/smart-home -B build-smart-home -DSMART_HOME_BUILD_TESTS=ON
cmake --build build-smart-home --config Release
ctest --test-dir build-smart-home -C Release --output-on-failure
```

| Published SDK check (2026-11-12) | MSVC 19.50, x64 Release | Offline CTest |
| --- | --- | --- |
| `ping-pong-example` | Compile/link passed | 0 registered tests |
| `echo-cpp` | Compile/link passed | 0 registered tests |
| `echo-capi` | Compile/link passed | 0 registered tests |
| `smart-home-app` | Compile/link passed | 1/1 passed |

Compilation does not establish cloud registration, live echo delivery or
ping-pong behavior. Those network scenarios were not run for these examples.

## SmartHome protocol

The example adapts the SDK's current push API to the original Java P2P protocol.
It retains method IDs 10/4/5/6, a leading uint32 request ID on request/reply
methods, result/error IDs 0/1, update ID 3, variant IDs 1–5 and the DTO field
order. The original C++ seconds timestamps remain unchanged.

The [offline fixtures](../cpp/smart-home/tests/README.md) freeze the old wire
bytes, collection-size boundary and optional-field masks. Tests also cover a
device completing after the parser handler is destroyed, scheduler cancellation,
and invalid IDs returning one error without accessing the device vector. A live
Java GUI/peer integration test was not run.

## Embedded and historical projects

All three firmware projects compiled and linked for ESP32-C6 with ESP-IDF 6.0.2
and GCC 15.2, fetching the exact SDK revision above without source overrides.
Dummy Wi-Fi configuration was used; no firmware was flashed.

| Firmware | ESP32-C6 compile/link | Application size | Smallest app partition free |
| --- | --- | --- | --- |
| C echo | Passed | `0x1bd350` bytes | 13% |
| C++ echo | Passed | `0x1ba8c0` bytes | 14% |
| SmartHome | Passed | `0x1d05b0` bytes | 9% |

Compilation does not verify board hardware, sensor readings, Wi-Fi
credentials/lifecycle or cloud delivery. Those runtime checks were not run.

The C++ echo ESP project also uses the published SDK pin; its former
`438-new-task-system` selector no longer resolved upstream. Auto-open projects
and the older temperature-sensor submodule retain
their existing SDK checkout/layout requirements. Arduino sketches use the
separately distributed vendored Arduino SDK. They are outside this standalone
telemetry dependency update and were not rebuilt here.
