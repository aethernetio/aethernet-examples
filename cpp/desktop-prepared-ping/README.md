# desktop-prepared-ping

Experimental prepared-packet example.

This example demonstrates:

1. Start the normal Aether client path.
2. Let Alice establish the normal client/server state.
3. Export PreparedSendMessageBlock for one target client.
4. Drop Alice's full send path.
5. Encode send_message packets directly in memory with EncodePacket.
6. Send the resulting bytes through a raw UDP datagram.
7. Bob receives ping.

The prepared-packet core intentionally does not own sockets, DNS, or platform transport.
It only produces packet bytes and advances the reserved nonce range.

Dependency:

CPMAddPackage(URI "https://github.com/aethernetio/aether-client-cpp.git#prepared-packet-v0")

Build:

cmake -S cpp/desktop-prepared-ping -B cpp/desktop-prepared-ping/build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cpp/desktop-prepared-ping/build-debug -j4
./cpp/desktop-prepared-ping/build-debug/prepared-ping-example
