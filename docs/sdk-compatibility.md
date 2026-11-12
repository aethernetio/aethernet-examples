# SDK compatibility checks

The desktop ping-pong, C++ echo and C echo projects use the published C++ SDK
revision recorded in their CMake dependency declarations. Local development can
override it with `-DCPM_aether-client-cpp_SOURCE=/path/to/aether-client-cpp`.

Verified SDK: `a7ec61a5db9af36cf6dcc3dee95ef489a09e470b`. The checks below used
fresh CPM caches and fetched that exact published revision and its pinned
dependencies, with no local source overrides.

Build each project in its own build directory, for example:

```sh
cmake -S cpp/desktop-ping-pong -B build-ping-pong
cmake --build build-ping-pong --config Release --target ping-pong-example
cmake -S cpp/echo_cpp -B build-echo-cpp
cmake --build build-echo-cpp --config Release --target echo-cpp
cmake -S cpp/echo_capi -B build-echo-capi
cmake --build build-echo-capi --config Release --target echo-capi
```

These projects do not register offline CTest suites. Successful compilation and
linking do not establish network delivery, cloud registration, persistent-state
restart, or embedded-board behavior. Those scenarios require their respective
runtime environments.

| Published SDK check (2026-11-12) | MSVC 19.50, x64 Release | Offline CTest discovery |
| --- | --- | --- |
| `ping-pong-example` | Compile/link passed | 0 registered tests |
| `echo-cpp` | Compile/link passed | 0 registered tests |
| `echo-capi` | Compile/link passed | 0 registered tests |

ESP-IDF/Arduino, cloud echo delivery and live ping-pong were not run in this
host-only check. Embedded projects retain their existing SDK branch selections.

## Smart-home and historical examples

`cpp/smart-home` does not currently compile against the current SDK. A Windows
MSVC compile on 2026-11-12 fails first in `src/api/api.h`: `ApiClassImpl`,
`PromiseResult`, `ApiClass` and `Method` belong to the previous RPC API. Device
implementations also use the previous `ActionContext`/`ActionPtr` API.
Its RPC contract with external smart-home peers needs a separate migration and
compatibility tests; this update does not change that protocol or its SDK ref.

Auto-open projects and the older temperature-sensor submodule retain their
existing SDK checkout/layout requirements. Arduino sketches use the separately
distributed vendored Arduino SDK. They are not covered by desktop build checks.
