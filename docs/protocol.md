# Custom ITCH-style Binary Protocol Specification

This project uses a deliberately custom, fixed-size, ITCH-style protocol inspired by exchange-feed engineering patterns. It is not NSE, BSE, Nasdaq ITCH, OUCH, or any other exchange protocol.

## Version and message types

* Protocol version: `1`
* Wire message size: `48` bytes
* Byte order: network byte order (big endian)
* Quantity: unsigned integer number of units
* Price: signed 64-bit integer `price_ticks` representing integer price ticks; this project does not attach a currency-specific decimal scale.

Message types:

| Code | Name | Semantics |
|---:|---|---|
| 1 | ADD_ORDER | Insert a new live order. |
| 2 | CANCEL_ORDER | Reduce the live order quantity by `quantity`. |
| 3 | EXECUTE_ORDER | Reduce the live order quantity by executed `quantity`. |
| 4 | REPLACE_ORDER | Replace the order's price and quantity while preserving `order_id`. |

A cancel or execute request larger than the live quantity removes the remaining quantity rather than creating negative depth. An invalid order reference is treated as a book update failure.

## Exact byte layout

| Offset | Field | Size | Encoding | Meaning |
|---:|---|---:|---|---|
| 0 | `message_length` | 2 | u16, big endian | Always `48` for this version. |
| 2 | `protocol_version` | 1 | u8 | Must be `1`. |
| 3 | `message_type` | 1 | u8 | `1..4` as above. |
| 4 | `sequence_number` | 8 | u64, big endian | Monotonically expected feed sequence. |
| 12 | `source_timestamp_ns` | 8 | u64, big endian | Source timestamp in nanoseconds. |
| 20 | `instrument_id` | 4 | u32, big endian | Positive instrument identifier. |
| 24 | `order_id` | 8 | u64, big endian | Positive order identifier. |
| 32 | `side` | 1 | u8 | `1=BUY`, `2=SELL`. |
| 33 | `reserved` | 3 | bytes | Must be zero in encoder; decoder ignores for forward compatibility. |
| 36 | `price_ticks` | 8 | i64, big endian | Positive integer price tick value. |
| 44 | `quantity` | 4 | u32, big endian | Positive event quantity. |

The implementation performs field-by-field encoding and decoding. It does not serialize C++ structs with `memcpy`, so compiler padding/alignment and host endianness do not leak onto the wire.

## UDP datagrams

A UDP datagram may contain one or more 48-byte messages. The feed handler walks the datagram in fixed-size message increments. A trailing partial message is treated as truncated/malformed and is not applied.

## Capture format

`feed_recorder` writes a framed binary capture with:

* magic: ASCII `MDCAP01`
* capture version: u16 big endian, currently `1`
* reserved: u16
* frame length: u32 big endian
* receive timestamp: u64 big endian
* original message bytes: exactly `frame length` bytes

Each valid decoded wire message gets its own capture frame. The receive timestamp is captured before decoding. Replay preserves the event bytes exactly and may optionally reproduce inter-frame timing.
