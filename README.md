# Æthernet Examples

Runnable examples for connecting embedded devices and applications through Æthernet.

The examples cover C++, ESP32, Java, and TypeScript integrations, including self-provisioning and direct message exchange between clients.

[Documentation](https://aethernet.io/documentation) · [Tutorials](https://aethernet.io/tutorial)

## Clone

Some examples use Git submodules:

```bash
git clone --recurse-submodules https://github.com/aethernetio/aethernet-examples.git
cd aethernet-examples
```

If the repository was cloned without submodules:

```bash
git submodule update --init --recursive
```

## SDKs

| Platform | SDK |
| --- | --- |
| C++ | [aether-client-cpp](https://github.com/aethernetio/aether-client-cpp) |
| Arduino / ESP32 | [aether-client-arduino-library](https://github.com/aethernetio/aether-client-arduino-library) |
| Java | [client-java](https://github.com/aethernetio/client-java) |
| TypeScript | [client-ts](https://github.com/aethernetio/client-ts) |

## Example categories

- basic client-to-client messaging;
- ESP32 and embedded-device integrations;
- environmental and temperature-sensor telemetry;
- application-side clients for receiving and displaying device data.

Each example directory contains the configuration and build files for its platform. Start with the README inside that directory when one is present.

## Credentials and device configuration

Never commit Wi-Fi passwords, production keys, registry credentials, or generated client state. Use local configuration files or environment variables and keep them outside version control.

## Reporting problems

Open an issue with the example path, operating system or board, toolchain version, build command, and complete error output.

## License

See [LICENSE](LICENSE).
