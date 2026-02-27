# EmbeddedSpacePacket

Minimal, embedded-optimized implementation of the **CCSDS Space Packet Protocol** in C,
following the international standard.

## Standards Compliance

- **CCSDS 133.0-B-2**: Space Packet Protocol

## Features

### Core Protocol Implementation

- **Primary Header**: 6-byte CCSDS-compliant header with version, type, APID, sequence
  control and packet length fields
- **Secondary Header**: Optional application-defined secondary header with configurable
  length
- **Sequence Control**: Segmentation flags (unsegmented, first, continuation, last) and
  14-bit sequence counter
- **CRC-16-CCITT**: Optional CRC over secondary header and payload, signalled by a flag
  in the secondary header
- **Serializer / Parser**: Symmetric encode and decode paths with big-endian network byte
  order

### Design Principles

- **Minimal footprint**: Single header + single source file
- **Zero allocation**: Stack-based, no dynamic memory in the library
- **Embedded-optimized**: No external dependencies, no OS primitives
- **Portable**: Pure C11, big-endian network byte order for all header fields
- **Safe**: Input validation on every public API entry point

## Project Structure

```
EmbeddedSpacePacket/
├── include/
│   └── space_packet.h       # Public API and types
├── src/
│   └── space_packet.c       # Serializer, parser, CRC and helpers
├── examples/
│   └── main.c               # Example: build, serialize and parse a packet
├── tests/
│   ├── cunit.h              # Minimal test framework
│   └── unit_tests.c         # Unit tests
├── scripts/
│   └── coverage_html.sh     # Coverage report
├── build/                   # Build artifacts 
├── Makefile
└── README.md
```

## Building

### Build Everything

```bash
make
```

Builds the static library, the example binary and the test binary in `build/`.

### Build Library Only

```bash
make lib
# Produces: build/libspacepacket.a
```

### Build Example

```bash
make example
./build/examples/spacepacket_example
```

### Run Tests

```bash
make ctest
./build/tests/ctest
```

### Coverage (HTML)

Requires `gcovr` installed in your system:

```bash
sudo apt install gcovr
```

Generate coverage report:

```bash
make coverage-html
```

Output report:

```bash
build/coverage/index.html
```

### Clean

```bash
make clean
```

## Quick Start

### Create and Serialize a Space Packet

```c
#include "space_packet.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const uint8_t payload[] = {'H', 'e', 'l', 'l', 'o', ' ', 'S', 'P'};

    /* Secondary header: flags byte (LSB=1 requests CRC), length byte (0 extra bytes) */
    const uint8_t sec_hdr[] = {0x01, 0x00};

    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt,
        0,                       /* version */
        0,                       /* type: telemetry */
        1,                       /* secondary header present */
        0x100,                   /* APID */
        SP_SEQ_FLAG_UNSEGMENTED, /* sequence flags */
        1                        /* sequence count */
    );
    sp_set_secondary_header(&pkt, sec_hdr, sizeof(sec_hdr));
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0) {
        printf("Serialization failed\n");
        return 1;
    }
    printf("Serialized %zu bytes\n", n);
    return 0;
}
```

### Parse an Incoming Packet

```c
sp_packet_t parsed;
if (sp_packet_parse(&parsed, buf, n)) {
    printf("APID: 0x%03X, seq count: %u, payload: %u bytes\n",
           parsed.ph.apid,
           parsed.ph.seq_count,
           parsed.payload_len);
} else {
    printf("Parse failed (bad length, corrupt CRC, etc.)\n");
}
```

### Compute a CRC Independently

```c
uint16_t crc = sp_crc16_ccitt(data, data_len);
printf("CRC-16-CCITT: 0x%04X\n", crc);
```

## API Reference

### Lifecycle

```c
/* Zero-initialize a packet structure. */
void sp_packet_init(sp_packet_t *pkt);
```

### Building a Packet

```c
/* Set primary header fields (values are masked to valid bit widths). */
void sp_set_primary_header(sp_packet_t *pkt,
                           uint8_t version,
                           uint8_t type,
                           uint8_t sec_hdr_flag,
                           uint16_t apid,
                           sp_seq_flag_t seq_flags,
                           uint16_t seq_count);

/* Set secondary header (pointer into application-owned memory). */
void sp_set_secondary_header(sp_packet_t *pkt,
                             const uint8_t *sec_hdr,
                             uint16_t sec_hdr_len);

/* Set payload (pointer into application-owned memory). */
void sp_set_payload(sp_packet_t *pkt,
                    const uint8_t *payload,
                    uint16_t payload_len);

/* Serialize to buffer. Returns bytes written, or 0 on error. */
size_t sp_packet_build_frame(const sp_packet_t *pkt,
                             uint8_t *buf,
                             size_t buf_len);
```

### Low-Level Serialization

```c
/* Required output buffer size for the given packet. */
size_t sp_packet_serialize_size(const sp_packet_t *pkt);

/* Serialize into buf. Returns bytes written, or 0 on error. */
size_t sp_packet_serialize(const sp_packet_t *pkt,
                           uint8_t *buf,
                           size_t buf_len);

/* Parse buf into out. out->payload points into buf (no copy). */
/* Returns 1 on success, 0 on failure (short buffer, bad CRC, etc.). */
int sp_packet_parse(sp_packet_t *out, uint8_t *buf, size_t buf_len);
```

### Utilities

```c
/* CRC-16-CCITT (polynomial 0x1021, init 0xFFFF). */
uint16_t sp_crc16_ccitt(const uint8_t *data, size_t len);
```

### Types

```c
typedef enum {
    SP_SEQ_FLAG_UNSEGMENTED        = 0,
    SP_SEQ_FLAG_FIRST_SEGMENT      = 1,
    SP_SEQ_FLAG_CONTINUING_SEGMENT = 2,
    SP_SEQ_FLAG_LAST_SEGMENT       = 3
} sp_seq_flag_t;
```

## Memory Usage (Estimated)

- **Library (stripped)**: < 2 KB
- **`sp_packet_t` struct**: ~40 bytes (stack)
- **Serialization buffer**: 6 bytes header + secondary header + payload + 2 bytes CRC
- **No heap usage**: all allocations are caller-supplied

## CCSDS Space Packet — Protocol Notes

- **Primary header (6 bytes):** bytes 0–1 hold version (3 bits), packet type (1 bit),
  secondary header flag (1 bit) and APID (11 bits). Bytes 2–3 are the sequence control
  field: segmentation flags (2 bits) and packet sequence count (14 bits). Bytes 4–5 are
  the Packet Length field (16 bits) which encodes the number of Packet Data Field octets
  minus 1.
- **Packet Data Field:** follows the primary header; contains an optional secondary
  header and user/application data.
- **Secondary header:** optional, application-defined. Presence is indicated by the
  secondary header flag. Format: byte 0 = flags (LSB = 1 requests CRC), byte 1 =
  number of additional secondary header bytes, followed by that many bytes.
- **Segmentation flags:** indicate whether a packet is standalone (`UNSEGMENTED`),
  or the first, continuation, or last segment of a segmented logical packet.
- **APID (Application Process ID):** 11-bit identifier for the source or consumer
  application / logical data stream.
- **CRC:** CCSDS Space Packet does not mandate CRC in the primary header. This library
  provides an opt-in CRC-16-CCITT covering the secondary header and payload, enabled by
  setting the LSB of the secondary header flags byte.

## Limitations

Current implementation focuses on core protocol features:

- No fragmentation / reassembly logic (sequence flag tracking is left to the application)
- CRC is only supported when a secondary header is present
- No thread safety (external locking required in multi-threaded contexts)
- Parser does not copy payload; the caller must keep the source buffer alive

## References

- [CCSDS 133.0-B-2: Space Packet Protocol](https://ccsds.org/Pubs/133x0b2e2.pdf)

## License

See LICENSE file.