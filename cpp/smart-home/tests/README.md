The fixed fixtures in `wire_test.cpp` freeze the original P2P contract, rather
than comparing two calls to the current serializer.

Sources used to derive the bytes:

- [Java schema](https://github.com/aethernetio/client-java/blob/main/smarthome/src/main/resources/smarthome.adsl.yaml): method IDs 10/4/5/6, update ID 3, variant IDs 1–5, DTO field order.
- [Original C++ stream](https://github.com/aethernetio/aether-client-cpp/blob/345e556b3ce310dc78201fde72da40937663c1d2/aether/mstream.h): arithmetic values, bool 00/01, string/vector size then contents, reflection order.
- [Original messages](https://github.com/aethernetio/aether-client-cpp/blob/345e556b3ce310dc78201fde72da40937663c1d2/aether/api_protocol/api_message.h): uint8 message ID and `TieredInt<uint64_t,uint8_t,250>` size encoding.
- [Original request ID](https://github.com/aethernetio/aether-client-cpp/blob/345e556b3ce310dc78201fde72da40937663c1d2/aether/api_protocol/request_id.h): uint32 ID.
- [Original reply API](https://github.com/aethernetio/aether-client-cpp/blob/345e556b3ce310dc78201fde72da40937663c1d2/aether/api_protocol/return_result_api.h): result ID0 followed by request/result; error ID1 followed by request/uint8 type/uint32 code. Error code 1 has the same bytes in the later int32 declaration.
- [Original nullable fields](https://github.com/aethernetio/aether-client-cpp/blob/345e556b3ce310dc78201fde72da40937663c1d2/aether/types/nullable_type.h): sensor mask precedes flattened fields, absent unit is bit2.

Fixtures cover each variant, LE integers/double, UTF-8, vector/optional layout,
the 250/251 size transition, exact request/reply/error/update payloads, deferred
completion after handler destruction, and invalid IDs without out-of-bounds
access. These are offline compatibility checks; they do not claim a live Java
GUI/network test. Timestamps intentionally retain the original C++ seconds value.
